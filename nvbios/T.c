/*
 * Copyright (C) 2016 Karol Herbst
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "bios.h"

static void envy_bios_parse_T_tmds_info(struct envy_bios *);

struct T_known_tables {
	uint8_t offset;
	uint16_t *ptr;
	const char *name;
};

static int
parse_at(struct envy_bios *bios, unsigned int idx, const char **name)
{
	struct envy_bios_T *t = &bios->T;
	struct T_known_tables tbls[] = {
		{ 0x0, &t->tmds_info.offset, "TMDS INFO TABLE" },
	};
	int entries_count = (sizeof(tbls) / sizeof(struct T_known_tables));

	/* check the index */
	if (idx >= entries_count)
		return -ENOENT;

	/* check the table has the right size */
	if (tbls[idx].offset + 2 > t->bit->t_len)
		return -ENOENT;

	if (name)
		*name = tbls[idx].name;

	return bios_u16(bios, t->bit->t_offset + tbls[idx].offset, tbls[idx].ptr);
}

int
envy_bios_parse_bit_T(struct envy_bios *bios, struct envy_bios_bit_entry *bit)
{
	struct envy_bios_T *t = &bios->T;
	unsigned int idx = 0;

	t->bit = bit;

	while (!parse_at(bios, idx, NULL))
		idx++;

	/* parse tables */
	envy_bios_parse_T_tmds_info(bios);

	return 0;
}

void
envy_bios_print_bit_T(struct envy_bios *bios, FILE *out, unsigned mask)
{
	struct envy_bios_T *t = &bios->T;
	uint16_t addr;
	int ret = 0, i = 0;

	if (!t->bit || !(mask & ENVY_BIOS_PRINT_T))
		return;

	fprintf(out, "BIT table 'T' at 0x%x, version %i\n",
		t->bit->offset, t->bit->version);

	for (i = 0; i * 2 < t->bit->t_len; ++i) {
		ret = bios_u16(bios, t->bit->t_offset + (i * 2), &addr);
		if (!ret && addr) {
			const char *name;
			ret = parse_at(bios, i, &name);
			fprintf(out, "0x%02x: 0x%x => %s\n", i * 2, addr, name);
		}
	}

	fprintf(out, "\n");
}

static void
envy_bios_parse_T_tmds_info(struct envy_bios *bios)
{
	struct envy_bios_T_tmds_info *tmds = &bios->T.tmds_info;
	int err = 0, i;

	if (!tmds->offset)
		return;

	bios_u8(bios, tmds->offset, &tmds->version);
	switch (tmds->version) {
	case 0x11:
	case 0x20:
		err |= bios_u8(bios, tmds->offset + 0x1, &tmds->hlen);
		err |= bios_u8(bios, tmds->offset + 0x2, &tmds->rlen);
		err |= bios_u8(bios, tmds->offset + 0x3, &tmds->entriesnum);
		tmds->valid = !err;
		break;
	default:
		ENVY_BIOS_ERR("Unknown TMDS INFO table version 0x%x\n", tmds->version);
		return;
	}

	tmds->entries = malloc(tmds->entriesnum * sizeof(struct envy_bios_T_tmds_info_entry));
	for (i = 0; i < tmds->entriesnum; ++i) {
		struct envy_bios_T_tmds_info_entry *e = &tmds->entries[i];
		e->offset = tmds->offset + tmds->hlen + i * tmds->rlen;
	}
}

void
envy_bios_print_T_tmds_info(struct envy_bios *bios, FILE *out, unsigned mask)
{
	struct envy_bios_T_tmds_info *tmds = &bios->T.tmds_info;
	int i;

	if (!tmds->offset || !(mask & ENVY_BIOS_PRINT_T))
		return;
	if (!tmds->valid) {
		ENVY_BIOS_ERR("Failed to parse TMDS INFO table at 0x%x, version %x\n\n", tmds->offset, tmds->version);
		return;
	}

	fprintf(out, "TMDS INFO table at 0x%x, version %x\n", tmds->offset, tmds->version);
	envy_bios_dump_hex(bios, out, tmds->offset, tmds->hlen, mask);
	if (mask & ENVY_BIOS_PRINT_VERBOSE) fprintf(out, "\n");

	for (i = 0; i < tmds->entriesnum; ++i) {
		struct envy_bios_T_tmds_info_entry *e = &tmds->entries[i];
		envy_bios_dump_hex(bios, out, e->offset, tmds->rlen, mask);
		if (mask & ENVY_BIOS_PRINT_VERBOSE) fprintf(out, "\n");
	}

	fprintf(out, "\n");
}
