void assemble( char * fpath )
{
    // Reset current segment
    curseg = &text;

    fp = fpath; // Save file path string
    curline = 0; // Reset current line
    p = NULL; // Set default value for p
    tk = TOK_EOL; // Set EOL token for readline inint
    fd = open( fpath, 0 ); // Open file for reading

    // Cannot open file
    if ( fd < 0 ) asmfail("failed to open file");

    ntok(); // Get first token and loop until EOF
    while ( tk )
    {
        // Label constant with indirect bit, or high byte, or low byte
        if ( tk == TOK_INDR || tk == TOK_BYLO || tk == TOK_BYHI )
        {
            ntok();
            if ( tk != TOK_NAME ) asmfail("expected label"); // The following token MUST be a label

            // Store ref to symbol
            struct asmsym * cursym = symtbl + tkVal;

            unsigned int val = cursym->val;
            // Set indirect bit if needed
            if ( tk == TOK_INDR ) val |= 0x8000;
            // This is a byte pointer
            else val << 1;
            // This is a high byte pointer
            if ( tk == TOK_BYHI ) val |= 1;

            // Compute relocation bits
            unsigned int rloc = tkVal << 4 | cursym->type & SYM_MASK | tk != TOK_INDR;

            // Add any following numbers
            ntok();
            if ( tk == TOK_MATH )
            {
                char doNeg = tkVal;

                ntok();
                if ( tk != TOK_NUM ) asmfail("expected numberical constant");

                if ( doNeg ) val -= tkVal;
                else val += tkVal;

                ntok();
            }

            segset( curseg, rloc, val );

            // Allocate room for symbol in this segment
            curseg->data.pos++;
        }
        // Label declaration or label constant
        else if ( tk == TOK_NAME )
        {
            // Store ref to symbol
            int cursymno = tkVal;
            struct asmsym * cursym = symtbl + cursymno;

            ntok();
            // Label declaration
            if ( tk == TOK_LABL )
            {
                if ( (cursym->type & SYM_MASK) == SYM_DEF ) // Undefined symbol
                {
                    cursym->type = curseg->sym | (cursym->type & SYM_GLOB);
                    cursym->val = curseg->data.pos;
                }
                else if ( ~flags & FLG_DATA ) // Already defined symbol
                {
                    asmfail("symbol already defined");
                }

                ntok();
                continue; // Go right into a new statement
            }
            // Some other token
            else
            {
                unsigned int val = cursym->val;

                // Add any following numbers
                if ( tk == TOK_MATH )
                {
                    char doNeg = tkVal;

                    ntok();
                    if ( tk != TOK_NUM ) asmfail("expected numerical constant");

                    if ( doNeg ) val -= tkVal;
                    else val += tkVal;

                    ntok();
                }

                // Write out symbol
                segset( curseg, cursymno << 4 | cursym->type & SYM_MASK, val );

                // Allocate room for this symbol in the segment
                curseg->data.pos++;
            }
        }
        else if ( tk == TOK_NUM || tk == TOK_MATH ) // Numerical constant
        {
            unsigned int val = 0;
            char doNeg;

            // Numerical constant with prefix: -1234 or +1234
            if ( tk == TOK_MATH )
            {
                doNeg = tkVal;

                ntok();
                if ( tk != TOK_NUM ) asmfail( "expected numerical constant" );

                if ( doNeg ) val = -tkVal;
                else val = tkVal;
            }

            ntok();
            if ( tk == TOK_MATH )
            {
                doNeg = tkVal;

                ntok();
                if ( tk != TOK_NUM ) asmfail( "expected numerical constant" );

                if ( doNeg ) val -= tkVal;
                else val += tkVal;

                ntok();
            }

            if ( curseg->sym == SYM_BSS ) // Increment pos for BSS segment
            {
                if ( val < 0 ) asmfail("cannot decrease size of BSS segment");
                curseg->data.pos += val;
            }
            else
            {
                segset( curseg, SYM_ABS, val );

                // Allocate room for absolute symbol in the segment
                curseg->data.pos++;
            }
        }
        else if ( tk == TOK_STR ) // User defined string
        {
            unsigned int val, i = 0;

            // Load string into segment
            while ( i < tkVal )
            {
                val = ustr[i++] & 0xFF;
                if ( i < tkVal ) val += (ustr[i++] & 0xFF) << 8;
                segset( curseg, SYM_ABS, val );
                curseg->data.pos++;
            }

            ntok();
        }
        else if ( tk >= DGN_IONO && tk <= DGN_CTAA ) // DGNova instruction
        {
            int optyp = tk; // Type of instruction
            int opval = tkVal; // The 16 bit instruction
            int oprlc = SYM_ABS; // Relocation bits for the instruction

            // I/O Instruction, need a device code and maybe an accumulator
            if ( optyp == DGN_IONO || optyp == DGN_IO || optyp == DGN_IOSK )
            {
                if ( optyp == DGN_IO )
                {
                    // Get accumulator
                    ntok();
                    if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected an accumulator");
                    opval |= tkVal << 11;
                    // Get comma
                    ntok();
                    if ( tk != TOK_ARG ) asmfail("expected a comma");
                }

                // Get I/O device address (Number of Hardware ID Constant)
                ntok();
                if ( (tk != TOK_NUM && tk != DGN_HWID) || tkVal < 0 || tkVal > 63 ) asmfail("expected I/O device address");
                opval |= tkVal;

                ntok();
            }
            // Flow Control Instruction, need displacement and maybe a mode
            // Load Instruction, need accumulator, displacement, and maybe a mode
            else if ( optyp == DGN_FLOW || optyp == DGN_LOAD )
            {
                ntok();

                if ( optyp == DGN_LOAD )
                {
                    // Get accumulator
                    if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected an accumulator");
                    opval |= tkVal << 11;
                    // Get comma
                    ntok();
                    if ( tk != TOK_ARG ) asmfail("expected a comma");
                    ntok();
                }

                // Get indirect flag
                if ( tk == TOK_INDR )
                {
                    opval |= 0b0000010000000000;
                    ntok();
                }

                // Get displacement
                if ( tk == TOK_NUM || tk == TOK_MATH ) // Numerical displacement (programmer can pick mode)
                {
                    // Store displacement
                    int disp = tkVal;
                    int mode = 0;

                    if ( tk == TOK_MATH )
                    {
                        ntok();
                        if ( tk != TOK_NUM ) asmfail("expected numerical constant");

                        if ( disp ) disp = -tkVal;
                        else disp = tkVal;
                    }

                    // Check if a mode was specified
                    ntok();
                    if ( tk == TOK_ARG )
                    {
                        // Get mode
                        ntok();
                        if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected an index mode");
                        mode = tkVal;

                        ntok();
                    }

                    if ( !mode && (disp < 0 || disp > 0xFF) ) // Zero page access
                    {
                        asmfail("invalid unsigned 8-bit zero page address");
                    }
                    else if ( mode && (disp < -128 || disp > 127) ) // Two's complement displacement
                    {
                        asmfail("invalid signed 8-bit displacement");
                    }

                    // Set mode and displacement
                    opval |= mode << 8;
                    opval |= disp;
                }
                else if ( tk == TOK_NAME ) // Label displacement (assembler picks mode)
                {
                    // Store ref to symbol
                    unsigned int cursymno = tkVal;
                    struct asmsym * cursym = symtbl + cursymno;

                    unsigned int val = 0;
                    ntok();
                    if ( tk == TOK_MATH )
                    {
                        char doNeg = tkVal;

                        ntok();
                        if ( tk != TOK_NUM ) asmfail("expected numerical constant");

                        if ( doNeg ) val -= tkVal;
                        else val += tkVal;

                        ntok();
                    }

                    // Convert undefined symbols to zdef
                    if ( flags & FLG_DATA && (cursym->type & SYM_MASK) == SYM_DEF )
                    {
                        cursym->type = SYM_ZDEF | ~(~cursym->type | SYM_MASK);
                    }

                    // Undefined zero page symbol
                    if ( (cursym->type & SYM_MASK) == SYM_ZDEF )
                    {
                        if ( val > 0xFF ) asmfail("label offset outside displacement range");

                        opval |= val;
                        oprlc = cursymno << 4 | SYM_ZDEF;
                    }
                    // Zero page displacement symbol
                    else if ( (cursym->type & SYM_MASK) == SYM_ZERO )
                    {
                        val += cursym->val;
                        if ( val > 0xFF ) asmfail("label offset outside displacement range");

                        opval |= val;
                        oprlc = cursymno << 4 | SYM_ZERO;
                    }
                    // Program counter relative symbol
                    else if ( (cursym->type & SYM_MASK) == curseg->sym )
                    {
                        int disp = cursym->val - curseg->data.pos + val;

                        // Out of bounds
                        if ( disp < -128 || disp > 127 ) asmfail("label outside displacement range");

                        // Output displacement and mode
                        opval |= disp & 0xFF | 0x100;
                    }
                    // Symbol not in current segment
                    else
                    {
                        asmfail("label not in current segment");
                    }
                }
            }
            // Arithmetic & Logic Instructions
            // Need two accumulators and maybe a skip code
            else if ( optyp == DGN_MATH )
            {
                // Get source accumulator
                ntok();
                if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected a source accumulator");
                opval |= tkVal << 13;

                // Get argument seperator
                ntok();
                if ( tk != TOK_ARG ) asmfail("expected a comma");

                // Get destination accumulator
                ntok();
                if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected a destination accumulator");
                opval |= tkVal << 11;

                // Check if there's a skip code
                ntok();
                if ( tk == TOK_ARG )
                {
                    ntok();

                    // Skip code cosntant
                    if ( tk == DGN_SKPC )
                    {
                        opval |= tkVal;
                    }
                    // Numerical skip code
                    else if ( tk == TOK_NUM && tkVal >= 0 && tkVal <= 7 )
                    {
                        opval |= tkVal;
                    }
                    else
                    {
                        asmfail("expected a skip condition");
                    }

                    ntok();
                }
                else if ( opval & 8 ) // Don't allow pseudo-trap instructions
                {
                    asmfail("arithmetic no-ops are not permitted");
                }
            }
            // Trap instruction
            // Needs an 11-bit trap number
            else if ( optyp == DGN_TRAP )
            {
                // Ensure this is an 11-bit numerical constant
                ntok();
                if ( tk != TOK_NUM || tk < 0 || tk > 2047 ) asmfail("invalid trap code");
                opval |= tkVal < 4;

                ntok();
            }
            // System Control Instruction (CPU, MMU, FPU, etc.)
            else if ( optyp >= DGN_CT && optyp <= DGN_CTAA )
            {
                if ( optyp == DGN_CTA || optyp == DGN_CTAF || optyp == DGN_CTAA )
                {
                    // Get accumulator
                    ntok();
                    if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected an accumulator");
                    opval |= tkVal << 11;

                    if ( optyp == DGN_CTAA )
                    {
                        ntok();
                        if ( tk != TOK_ARG ) asmfail("expected a comma"); // Missing argument seperator

                        // Get second accumulator
                        ntok();
                        if ( tk != TOK_NUM || tkVal < 0 || tkVal > 3 ) asmfail("expected a second accumulator");
                        opval |= tkVal << 6;
                    }
                }

                ntok();
            }

            segset( curseg, oprlc, opval );

            // Write out instruction
            curseg->data.pos++;
        }
        // Assembler .text directive
        else if ( tk == ASM_TEXT ) { curseg = &text; ntok(); }
        // Assembler .data directive
        else if ( tk == ASM_DATA ) { curseg = &data; ntok(); }
        // Assembler  .bss directive
        else if ( tk == ASM_BSS  ) { curseg = &bss;  ntok(); }
        // Assembler .zero directive
        else if ( tk == ASM_ZERO ) { curseg = &zero; ntok(); }
        // Assembler .glob directive
        else if ( tk == ASM_GLOB ) while ( tk == ASM_GLOB || tk == TOK_ARG )
        {
            // Get label
            ntok();
            if ( tk != TOK_NAME ) asmfail("expected a label");

            // Make label global
            symtbl[tkVal].type |= SYM_GLOB;

            ntok(); // Get comma or next statement
        }
        // Assembler .define directive
        else if ( tk == ASM_DEFN )
        {
            // Get label
            ntok();
            if ( tk != TOK_NAME ) asmfail("expected a label");

            // Store ref to symbol
            struct asmsym * cursym = symtbl + tkVal;

            // Already defined symbol
            if ( (cursym->type & SYM_MASK) != SYM_DEF ) asmfail("label already defined");

            // Get comma
            ntok();
            if ( tk != TOK_ARG ) asmfail("expected a comma");

            // Get numerical value to assign
            ntok();
            if ( tk != TOK_NUM ) asmfail("expected a constant value");

            // Assign value to absolute symbol
            cursym->type = SYM_ABS | SYM_GLOB;
            cursym->val = tkVal;

            ntok();
        }
        // Assembler .ent directive
        else if ( tk == ASM_ENT )
        {
            ntok();
            if ( tk != TOK_NAME ) asmfail("expected a label");

            if ( flags & FLG_DATA )
            {
                struct asmsym * cursym = symtbl + tkVal;

                if ( (cursym->type & SYM_MASK) != SYM_TEXT ) asmfail("label must be a text label");

                // Update entry position
                entrypos = cursym->val;
            }

            ntok();
        }
        // Assembler .wstr directive (word string)
        else if ( tk == ASM_WSTR )
        {
            if ( curseg == &bss ) asmfail("cannot write word string to bss segment");

            ntok();
            if ( tk != TOK_STR ) asmfail("expected a string");

            unsigned int i = 0;
            while ( i < tkVal )
            {
                segset( curseg, SYM_ABS, ustr[i++] & 0xFF );
                curseg->data.pos++;
            }

            ntok();
        }
        // This must be last!
        else if ( tk != TOK_EOL )
        {
            asmfail("invalid token");
        }

        // Make sure this is the correct end of statement
        if ( tk != TOK_EOL )
        {
            asmfail("expected end of statement");
        }

        ntok();
    }

    // Close the file when done
    close( fd );
}