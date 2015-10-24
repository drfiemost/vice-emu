/*
 * genamigaintl - amiga intl.h and intl_table.h generation helper program.
 *
 * Written by
 *  Marco van den Heuvel <blackystardust68@yahoo.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char line_buffer[512];

int get_line(FILE *file)
{
    char c = 0;
    int counter = 0;

    while (c != '\n' && !feof(file) && counter < 511) {
        c = fgetc(file);
        if (c != 0xd) {
            line_buffer[counter++] = c;
        }
    }
    if (counter > 1) {
        line_buffer[counter - 1] = 0;
    }

    if (feof(file)) {
        return -1;
    }

    return counter - 1;
}

static char *vice_stralloc(const char *str)
{
    size_t size;
    char *ptr;

    if (str == NULL) {
        exit(-1);
    }

    size = strlen(str) + 1;
    ptr = malloc(size);

    memcpy(ptr, str, size);

    return ptr;
}

static void generate_intl(char *in_filename, char *out_filename, char *table_filename)
{
    FILE *infile = NULL;
    FILE *outfile = NULL;
    FILE *outfile_table = NULL;
    int line_size;
    char *header;
    char *table_header;

    infile = fopen(in_filename, "rb");
    if (infile == NULL) {
        printf("cannot open %s for reading\n", infile);
        return;
    }

    outfile = fopen(out_filename, "wb");
    if (outfile == NULL) {
        printf("cannot open %s for writing\n", out_filename);
        fclose(infile);
        return;
    }

    outfile_table = fopen(table_filename, "wb");
    if (outfile_table == NULL) {
        printf("cannot open %s for writing\n", table_filename);
        fclose(infile);
        fclose(outfile);
        return;
    }

    header = "/*\n"
             " * intl.h - Localization routines for Amiga.\n"
             " *\n"
             " * Autogenerated by genintl_h.sh, DO NOT EDIT !!!\n"
             " *\n"
             " * Written by\n"
             " *  Marco van den Heuvel <blackystardust68@yahoo.com>\n"
             " *\n"
             " * This file is part of VICE, the Versatile Commodore Emulator.\n"
             " * See README for copyright notice.\n"
             " *\n"
             " *  This program is free software; you can redistribute it and/or modify\n"
             " *  it under the terms of the GNU General Public License as published by\n"
             " *  the Free Software Foundation; either version 2 of the License, or\n"
             " *  (at your option) any later version.\n"
             " *\n"
             " *  This program is distributed in the hope that it will be useful,\n"
             " *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
             " *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
             " *  GNU General Public License for more details.\n"
             " *\n"
             " *  You should have received a copy of the GNU General Public License\n"
             " *  along with this program; if not, write to the Free Software\n"
             " *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA\n"
             " *  02111-1307  USA.\n"
             " *\n"
             " */\n"
             "\n"
             "#ifndef VICE_INTL_H\n"
             "#define VICE_INTL_H\n"
             "\n"
             "#include \"intl_funcs.h\"\n"
             "\n"
             "enum { ID_START_0=0,\n"
             "\n";

    table_header = "/*\n"
                   " * intl_table.h - Translation table for Amiga.\n"
                   " *\n"
                   " * Autogenerated by genintltable.sh, DO NOT EDIT !!!\n"
                   " *\n"
                   " * Written by\n"
                   " *  Marco van den Heuvel <blackystardust68@yahoo.com>\n"
                   " *\n"
                   " * This file is part of VICE, the Versatile Commodore Emulator.\n"
                   " * See README for copyright notice.\n"
                   " *\n"
                   " *  This program is free software; you can redistribute it and/or modify\n"
                   " *  it under the terms of the GNU General Public License as published by\n"
                   " *  the Free Software Foundation; either version 2 of the License, or\n"
                   " *  (at your option) any later version.\n"
                   " *\n"
                   " *  This program is distributed in the hope that it will be useful,\n"
                   " *  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                   " *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                   " *  GNU General Public License for more details.\n"
                   " *\n"
                   " *  You should have received a copy of the GNU General Public License\n"
                   " *  along with this program; if not, write to the Free Software\n"
                   " *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA\n"
                   " *  02111-1307  USA.\n"
                   " *\n"
                   " */\n"
                   "\n"
                   "static int intl_translate_text_table[][countof(language_table)] = {\n";

    fprintf(outfile, header);
    fprintf(outfile_table, table_header);

    line_size = get_line(infile);

    while (line_size != -1) {
        if (line_buffer[0] == 'I' && line_buffer[1] == 'D') {
            fprintf(outfile, "%s,\n", line_buffer);
            fprintf(outfile, "%s_DA,", line_buffer);
            fprintf(outfile, "%s_DE,", line_buffer);
            fprintf(outfile, "%s_ES,", line_buffer);
            fprintf(outfile, "%s_FR,", line_buffer);
            fprintf(outfile, "%s_HU,", line_buffer);
            fprintf(outfile, "%s_IT,", line_buffer);
            fprintf(outfile, "%s_KO,", line_buffer);
            fprintf(outfile, "%s_NL,", line_buffer);
            fprintf(outfile, "%s_PL,", line_buffer);
            fprintf(outfile, "%s_RU,", line_buffer);
            fprintf(outfile, "%s_SV,", line_buffer);
            fprintf(outfile, "%s_TR,", line_buffer);
            fprintf(outfile_table, "/* en */ {%s,\n", line_buffer);
            fprintf(outfile_table, "/* da */ %s_DA,\n", line_buffer);
            fprintf(outfile_table, "/* de */ %s_DE,\n", line_buffer);
            fprintf(outfile_table, "/* es */ %s_ES,\n", line_buffer);
            fprintf(outfile_table, "/* fr */ %s_FR,\n", line_buffer);
            fprintf(outfile_table, "/* hu */ %s_HU,\n", line_buffer);
            fprintf(outfile_table, "/* it */ %s_IT,\n", line_buffer);
            fprintf(outfile_table, "/* ko */ %s_KO,\n", line_buffer);
            fprintf(outfile_table, "/* nl */ %s_NL,\n", line_buffer);
            fprintf(outfile_table, "/* pl */ %s_PL,\n", line_buffer);
            fprintf(outfile_table, "/* ru */ %s_RU,\n", line_buffer);
            fprintf(outfile_table, "/* sv */ %s_SV,\n", line_buffer);
            fprintf(outfile_table, "/* tr */ %s_TR},\n\n", line_buffer);
        } else {
            if (line_size) {
                fprintf(outfile, "%s\n", line_buffer);
                fprintf(outfile_table, "%s\n", line_buffer);
            } else {
                fprintf(outfile, "\n");
                fprintf(outfile_table, "\n");
            }
        }
        line_size = get_line(infile);
    }

    fprintf(outfile, "};\n");
    fprintf(outfile, "#endif\n");
    fclose(outfile);
    fclose(outfile_table);
    fclose(infile);
}

int main(int argc, char *argv[])
{
    int i;

    if (argc < 3) {
        printf("too few arguments\n");
        exit(1);
    }

    /* argv[1] = intl.txt file for reading */
    /* argv[2] = intl.h file for writing */
    /* argv[3] = intl_table.h file for writing */

    generate_intl(argv[1], argv[2], argv[3]);

    return 0;
}
