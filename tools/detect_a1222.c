/*
 * detect_a1222.c - Hardware architecture detection tool for AmigaOS 4
 * Detects whether the current system is an AmigaOne A1222 (Tabor e500v2 SPE).
 *
 * Copyright (C) 2026 Bill Borsari
 *
 * SPDX-License-Identifier: MIT
 */

#include <proto/exec.h>
#include <proto/expansion.h>
#include <expansion/expansion.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    struct Library *ExpansionBase = IExec->OpenLibrary("expansion.library", 50);
    if (!ExpansionBase) {
        return 5;
    }

    struct ExpansionIFace *IExpansion = (struct ExpansionIFace *)IExec->GetInterface(ExpansionBase, "main", 1, NULL);
    if (!IExpansion) {
        IExec->CloseLibrary(ExpansionBase);
        return 5;
    }

    uint32 machine = 0;
    CONST_STRPTR machine_name = NULL;
    IExpansion->GetMachineInfoTags(
        GMIT_Machine, &machine,
        GMIT_MachineString, &machine_name,
        TAG_END
    );

    int is_a1222 = (machine == MACHINETYPE_A1222);

    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'v') {
        printf("Machine code: %u (%s)\n", (unsigned int)machine, machine_name ? (const char *)machine_name : "unknown");
        printf("A1222 detected: %s\n", is_a1222 ? "YES" : "NO");
    }

    IExec->DropInterface((struct Interface *)IExpansion);
    IExec->CloseLibrary(ExpansionBase);

    return is_a1222 ? 0 : 5;
}
