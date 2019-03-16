/*
 * Copyright (c) 2019 Christian Mauderer <oss@c-mauderer.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "crc32.h"

#define HEADER_LEN 264 /* includes padding and CRC */
#define CRC_SIZE 4
#define PADDING_SIZE 4
#define END_LEN (3*4)
#define END_STRING "END."

#define KERNEL_PART_SIZE    0x00040000ul
#define KERNEL_PART_NAME    "kernel"
#define KERNEL_PART_ADDRESS 0x9f050000ul
#define ROOTFS_PART_SIZE    0x00b60000ul
#define ROOTFS_PART_NAME    "rootfs"
#define ROOTFS_PART_ADDRESS 0x9f045000ul

struct part_hdr {
	char type[4]; /* "PART" for the partitions. */
	char part_name[28]; /* For "PART": Name of the partition. */
	char unknown1[4];
	char header_number[4];
	char start_address[4];
	char unknown2[4];
	char content_size[4];
	char max_size[4];
};

static void printhelp(void)
{
	fprintf(stderr,
	    "Inofficial Ubiquiti airCube image generator.\n"
	    "Possible options:\n"
	    "  -h        Print this help.\n"
	    "  -i <in>   Use <in> as input file.\n"
	    "  -o <out>  Use <out> as output file.\n"
	    );
}

static void format_uint32(char *target, uint32_t val)
{
	target[0] = (val >> 24) & 0xff;
	target[1] = (val >> 16) & 0xff;
	target[2] = (val >>  8) & 0xff;
	target[3] = (val      ) & 0xff;
}

static void format_int32(char *target, int32_t val)
{
	format_uint32(target, (uint32_t) val);
}

static void write_header(int fdout, crc_t *crc)
{
	char header[HEADER_LEN] = "OPENWRT.image";
	crc_t header_crc;

	header_crc = crc_init();
	header_crc = crc_update(header_crc, header, HEADER_LEN - CRC_SIZE);
	header_crc = crc_finalize(header_crc);
	format_int32(&header[HEADER_LEN - CRC_SIZE], header_crc);

	*crc = crc_update(*crc, header, HEADER_LEN);
	write(fdout, header, HEADER_LEN);
}

static bool write_partition(
    int fdout,
    crc_t *crc,
    int fdin,
    uint32_t address,
    ssize_t maxsize,
    const char *part_name,
    uint32_t number
)
{
	char *bigbuf;
	ssize_t size;
	bool done = false;
	struct part_hdr hdr = {
	    .type={'P', 'A', 'R', 'T'},
	};
	crc_t part_crc;

	bigbuf = calloc(maxsize + CRC_SIZE + PADDING_SIZE, 1);
	if (bigbuf == NULL) {
		err(1, "Couldn't allocate buffer");
	}

	size = read(fdin, bigbuf, maxsize);
	if (size < 0) {
		err(1, "Couldn't read from file");
	}
	if (size < maxsize) {
		done = true;
	}
	/* FIXME: This is a workaround for unknown CRC calculation with length
	 * not dividable by four. */
	if (size % 4 != 0) {
		size = (size | 0x3) + 1;
	}
	if (size > maxsize) {
		errx(1,
		    "size (%zd) got bigger than maxsize (%zd). That shouldn't happen for a sane maxsize.",
		    size, maxsize);
	}

	strncpy(hdr.part_name, part_name, sizeof(hdr.part_name)-1);
	format_uint32(hdr.header_number, number);
	format_uint32(hdr.start_address, address);
	format_uint32(hdr.max_size, maxsize);
	format_int32(hdr.content_size, size);

	part_crc = crc_init();
	part_crc = crc_update(part_crc, &hdr, sizeof(hdr));
	*crc = crc_update(*crc, &hdr, sizeof(hdr));
	write(fdout, &hdr, sizeof(hdr));

	part_crc = crc_update(part_crc, bigbuf, size);
	part_crc = crc_finalize(part_crc);
	format_int32(&bigbuf[size], part_crc);

	*crc = crc_update(*crc, bigbuf, size + CRC_SIZE + PADDING_SIZE);
	write(fdout, bigbuf, size + CRC_SIZE + PADDING_SIZE);

	free(bigbuf);
	return done;
}

static void write_end(int fdout, crc_t *crc)
{
	char end[END_LEN] = END_STRING;

	*crc = crc_update(*crc, end, sizeof(END_STRING));
	*crc = crc_finalize(*crc);
	format_int32(&end[sizeof(END_STRING)], *crc);

	write(fdout, end, END_LEN);
}

int main(int argc, char *argv[])
{
	char *infile = NULL;
	char *outfile = NULL;
	int c;
	int fdin;
	int fdout;
	crc_t crc;
	bool done;

	while ((c = getopt (argc, argv, "hi:o:")) != -1)
	switch (c)
	{
	case 'h':
		printhelp();
		exit(0);
		break;
	case 'i':
		infile = strdup(optarg);
		break;
	case 'o':
		outfile = strdup(optarg);
		break;
	case '?':
		errx(1, "Argument error.");
		break;
	default:
		errx(1, "Unknown error.");
		break;
	}

	if (infile == NULL) {
		fdin = STDIN_FILENO;
	} else {
		fdin = open(infile, O_RDONLY);
		if (fdin == -1) {
			err(1, "Error opening input file \"%s\"", infile);
		}
	}

	if (outfile == NULL) {
		fdout = STDOUT_FILENO;
	} else {
		fdout = open(outfile, O_WRONLY | O_CREAT);
		if (fdout == -1) {
			err(1, "Error opening output file \"%s\"", outfile);
		}
	}

	crc = crc_init();
	write_header(fdout, &crc);
	done = write_partition(fdout, &crc, fdin,
	    KERNEL_PART_ADDRESS, KERNEL_PART_SIZE, KERNEL_PART_NAME, 0);
	if (!done) {
		done = write_partition(fdout, &crc, fdin,
		    ROOTFS_PART_ADDRESS, ROOTFS_PART_SIZE, ROOTFS_PART_NAME, 1);
	}
	write_end(fdout, &crc);

	close(fdout);
	close(fdin);

	return 0;
}
