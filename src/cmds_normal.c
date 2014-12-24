#include <ctype.h>   // for isdigit
#include <stdlib.h>  // for atoi
#include "yank.h"
#include "marks.h"
#include "cmds.h"
#include "conf.h"
#include "screen.h"
#include "color.h"   // for set_ucolor
#include "cmds_edit.h"
#include "history.h"
#include "hide_show.h"
#include "undo.h"
#include "shift.h"
#include "interp.h"
#include "utils/extra.h"

extern int cmd_multiplier;
extern struct history * commandline_history;
extern void start_visualmode(int tlrow, int tlcol, int brrow, int brcol);
char interp_line[100];

void do_normalmode(struct block * buf) {
    int bs = get_bufsize(buf);
    struct ent * e;

    switch (buf->value) {

        // Movement commands
        case 'j':
        case OKEY_DOWN:
            currow = forw_row(1)->row; 
            unselect_ranges();
            update();
            break;

        case 'k':
        case OKEY_UP:
            currow = back_row(1)->row;
            unselect_ranges();
            update();
            break;

        case 'h':
        case OKEY_LEFT:
            curcol = back_col(1)->col;
            unselect_ranges();
            update();
            break;

        case 'l':
        case OKEY_RIGHT:
            curcol = forw_col(1)->col;
            unselect_ranges();
            update();
            break;

        case '0':
        case OKEY_HOME:
            curcol = left_limit()->col;
            unselect_ranges();
            update();
            break;

        case '$':
        case OKEY_END:
            curcol = right_limit()->col;
            unselect_ranges();
            update();
            break;

        case '^':
            currow = goto_top()->row;
            unselect_ranges();
            update();
            break;

        case '#':
            currow = goto_bottom()->row;
            unselect_ranges();
            update();
            break;

        // Tick
        case '\'':
            if (bs != 2) break;
            unselect_ranges();
            e = tick(buf->pnext->value);
            if (row_hidden[e->row]) {
                error("Cell row is hidden");
                break;
            }
            if (col_hidden[e->col]) {
                error("Cell column is hidden");
                break;
            }
            currow = e->row;
            curcol = e->col;
            update();
            break;

        // CTRL f
        case ctl('f'):
            {
            int n = LINES - RESROW - 1;
            if (atoi(get_conf_value("half_page_scroll"))) n = n / 2;
            struct ent * e = forw_row(n);
            currow = e->row;
            unselect_ranges();
            scroll_down(n);
            update();
            break;
            }

        // CTRL b
        case ctl('b'):
            {
            int n = LINES - RESROW - 1;
            if (atoi(get_conf_value("half_page_scroll"))) n = n / 2;
            currow = back_row(n)->row; 
            unselect_ranges();
            scroll_up(n);
            update();
            break;
            }

        case 'w':
            e = go_forward();  
            currow = e->row;
            curcol = e->col;
            unselect_ranges();
            update();
            break;

        case 'b':
            e = go_backward();  
            currow = e->row;
            curcol = e->col;
            unselect_ranges();
            update();
            break;

        case '/':
            {
            char cadena[] = ":int goto ";
            int i;
            for (i=0; i<strlen(cadena); i++) {
                flush_buf(buf);
                addto_buf(buf, cadena[i]);
                exec_single_cmd(buf);
            }
            break;
            }

        case 'H':
            currow = vert_top()->row;
            unselect_ranges();
            update();
            break;

        case 'M':
            currow = vert_middle()->row;
            unselect_ranges();
            update();
            break;

        case 'L':
            currow = vert_bottom()->row;
            unselect_ranges();
            update();
            break;

        case 'G': // goto end
            e = go_end();  
            currow = e->row;
            curcol = e->col;
            unselect_ranges();
            update();
            break;

        // GOTO goto
        case ctl('a'):
            e = go_home(); 
            curcol = e->col;
            currow = e->row;
            unselect_ranges();
            update();
            break;

        case 'g':
            if (buf->pnext->value == '0') {                               // g0
                curcol = go_bol()->col;

            } else if (buf->pnext->value == '$') {                        // g$
                curcol = go_eol()->col;

            } else if (buf->pnext->value == 'g') {                        // gg
                e = go_home(); 
                curcol = e->col;
                currow = e->row;

            } else if (buf->pnext->value == 'G') {                        // gG
                e = go_end();  
                currow = e->row;
                curcol = e->col;

            } else if (buf->pnext->value == 'M') {                        // gM
                curcol = horiz_middle()->col;

            } else {                                                      // gA4 (goto cell)
                (void) sprintf(interp_line, "goto %s", parse_cell_name(1, buf));
                send_to_interp(interp_line);
            }
            unselect_ranges();
            update();
            break;

        // repeat last command
        case '.':
            copybuffer(lastcmd_buffer, buf); // nose graba en lastcmd_buffer!!
            cmd_multiplier = 1;
            exec_mult(buf, COMPLETECMDTIMEOUT);
            break;

        // enter command mode
        case ':':
            clr_header(input_win, 0);
            chg_mode(':');
            add(commandline_history, "");
            print_mode(input_win);
            wrefresh(input_win);
            handle_cursor();
            inputline_pos = 0;
            break;

        // enter visual mode
        case 'v':
            chg_mode('v');
            handle_cursor();
            clr_header(input_win, 0);
            print_mode(input_win);
            wrefresh(input_win);
            start_visualmode(currow, curcol, currow, curcol);
            break;

        // INPUT COMMANDS
        case '=':
        case '\\':
        case '<':
        case '>':
            if (locked_cell(currow, curcol)) return;
            insert_edit_submode = buf->value;
            chg_mode(insert_edit_submode);
            clr_header(input_win, 0);
            print_mode(input_win);
            wrefresh(input_win);
            inputline_pos = 0;
            break;

        // EDITION COMMANDS
        // edit cell (v)
        case 'e':
            if (locked_cell(currow, curcol)) return;
            clr_header(input_win, 0);
            inputline_pos = 0;
            if (start_edit_mode(buf, 'v')) show_header(input_win);
            break;

        // edit cell (s)
        case 'E':
            if (locked_cell(currow, curcol)) return;
            clr_header(input_win, 0);
            inputline_pos = 0;
            if (start_edit_mode(buf, 's')) show_header(input_win);
            else {
                info("No string value to edit");
                chg_mode('.');
                show_celldetails(input_win);
                print_mode(input_win);
                wrefresh(input_win);
            }
            break;

        // del current cell or range
        case 'x': 
            del_selected_cells();
            update();
            break;

        // format col
        case 'f': 
            if (bs != 2) return;
            formatcol(buf->pnext->value);
            break;

        // mark cell or range
        case 'm': 
            if (bs != 2) break;                
            int p = is_range_selected();
            if (p != -1) { // mark range
                struct srange * sr = get_range_by_pos(p); 
                set_range_mark(buf->pnext->value, sr);                   
            } else         // mark cell 
                set_cell_mark(buf->pnext->value, currow, curcol);
            break;

        // copy
        case 'c': 
            {
            if (bs != 2) break;
            struct ent * p = *ATBL(tbl, get_mark(buf->pnext->value)->row, get_mark(buf->pnext->value)->col);
            int c1;
            struct ent * n;
            cmd_multiplier++;

            for (c1 = curcol; cmd_multiplier-- && c1 < maxcols; c1++) {
                if ((n = * ATBL(tbl, currow, c1))) {
                    if (n->flags & is_locked)
                        continue;
                    if (!p) {
                        clearent(n);
                        continue;
                    }
                } else {
                    if (!p) break;
                    n = lookat(currow, c1);
                }
                copyent(n, p, currow - get_mark(buf->pnext->value)->row, c1 - get_mark(buf->pnext->value)->col, 0, 0, maxrow, maxcol, 0);

                n->row += currow - get_mark(buf->pnext->value)->row;
                n->col += c1 - get_mark(buf->pnext->value)->col;

                n->flags |= is_changed;
            }
            if (atoi(get_conf_value("autocalc"))) EvalAll();
            update();
            break;
            }

        // repeat last goto command
        case 'n':
            go_last();
            update();
            break;

        // create range with two marks
        case 'r':  
            if (bs == 3) {
                create_range(buf->pnext->value, buf->pnext->pnext->value);
                update();
            }
            break;

        // Zr Zc - Zap col or row - Show col or row - Sr Sc
        case 'Z':
        case 'S':
            ; int rs, r = currow, c = curcol, arg = cmd_multiplier + 1;
            struct srange * sr;
            if ( (rs = is_range_selected()) != -1) {
                sr = get_range_by_pos(rs);
                cmd_multiplier = 1;
                r = sr->tlrow;
                c = sr->tlcol;
                arg = buf->pnext->value == 'r' ? sr->brrow - sr->tlrow + 1 : sr->brcol - sr->tlcol + 1;
            }
            if (buf->value == 'Z' && buf->pnext->value == 'r') {
                hide_row(r, arg);
            } else if (buf->value == 'Z' && buf->pnext->value == 'c') {
                hide_col(c, arg);
            } else if (buf->value == 'S' && buf->pnext->value == 'r') {
                show_row(r, arg);
            } else if (buf->value == 'S' && buf->pnext->value == 'c') {
                show_col(c, arg);
            }
            cmd_multiplier = 0;
            update();
            break;

        // shift range or cell
        case 's':
            {
            int p, r = currow, c = curcol, rf = currow, cf = curcol;
            if ( (p = is_range_selected()) != -1) {
                struct srange * sr = get_range_by_pos(p);
                r = sr->tlrow;
                c = sr->tlcol;
                rf = sr->brrow;
                cf = sr->brcol;
            }
            if ( any_locked_cells(r, c, rf, cf) && (buf->pnext->value == 'h' || buf->pnext->value == 'k') ) {
                error("Locked cells encountered. Nothing changed");           
                return;
            }
            create_undo_action();
            int ic = cmd_multiplier + 1;
            switch (buf->pnext->value) {
                case 'j':
                    fix_marks(  rf - r + 1  , 0, r, maxrow, c, cf);
                    save_undo_range_shift(rf - r + 1, 0   , r, c, rf, cf);
                    shift_range(rf - r + 1, 0             , r, c, rf, cf);
                    break;
                case 'k':
                    fix_marks( -(rf - r + 1), 0, r, maxrow, c, cf);
                    yank_area(r, c, rf, cf, 'a', ic); // keep ents in yanklist for sk
                    copy_to_undostruct(r, c, rf, cf, 'd');
                    save_undo_range_shift(-(rf - r + 1), 0, r, c, rf, cf);
                    shift_range(-(rf - r + 1), 0          , r, c, rf, cf);
                    //copy_to_undostruct(r, c, rf, cf, 'a');
                    break;
                case 'h':
                    fix_marks(0, -(cf - c + 1), r, rf, c, maxcol);
                    yank_area(r, c, rf, cf, 'a', ic); // keep ents in yanklist for sh
                    copy_to_undostruct(r, c, rf, cf, 'd');
                    save_undo_range_shift(0, -(cf - c + 1), r, c, rf, cf);
                    shift_range(0, - (cf - c + 1)         , r, c, rf, cf);
                    //copy_to_undostruct(r, c, rf   , cf, 'a');
                    break;
                case 'l':
                    fix_marks(0, cf - c + 1, r, rf, c, maxcol);
                    save_undo_range_shift(0, cf - c + 1   , r, c, rf, cf);
                    shift_range(0, cf - c + 1             , r, c, rf, cf);
                    break;
            }
            end_undo_action();
            cmd_multiplier = 0;
            unselect_ranges();
            update();
            break;
            }

        // delete row or column, or selected cell or range
        case 'd':
            {
            if (bs != 2) return;
            int ic = cmd_multiplier + 1;

            if (buf->pnext->value == 'r') {
                if (any_locked_cells(currow, 0, currow + cmd_multiplier, maxcol)) {
                    error("Locked cells encountered. Nothing changed");           
                    return;
                }
                create_undo_action();
                copy_to_undostruct(currow, 0, currow - 1 + ic, maxcol, 'd');
                save_undo_range_shift(-ic, 0, currow, 0, currow -1 + ic, maxcol);
                fix_marks(-ic, 0, currow - 1 + ic, maxrow, 0, maxcol);
                yank_area(currow, 0, currow + cmd_multiplier, maxcol, 'r', ic);
                while (ic--) deleterow();
                copy_to_undostruct(currow, 0, currow + cmd_multiplier, maxcol, 'a');
                if (cmd_multiplier > 0) cmd_multiplier = 0;
                end_undo_action();

            } else if (buf->pnext->value == 'c') {
                if (any_locked_cells(0, curcol, maxrow, curcol + cmd_multiplier)) {
                    error("Locked cells encountered. Nothing changed");           
                    return;
                }
                create_undo_action();
                copy_to_undostruct(0, curcol, maxrow, curcol - 1 + ic, 'd');
                save_undo_range_shift(0, -ic, 0, curcol, maxrow, curcol - 1 + ic);
                fix_marks(0, -ic, 0, maxrow,  curcol - 1 + ic, maxcol);
                yank_area(0, curcol, maxrow, curcol + cmd_multiplier, 'c', ic);
                while (ic--) deletecol();
                copy_to_undostruct(0, curcol, maxrow, curcol + cmd_multiplier, 'a');
                if (cmd_multiplier > 0) cmd_multiplier = 0;
                end_undo_action();

            } else if (buf->pnext->value == 'd') {
                del_selected_cells(); 
            }
            update();
            break;
            }

        // insert row or column
        case 'i':
            {
            if (bs != 2) return;
            create_undo_action();
            if (buf->pnext->value == 'r') {
                save_undo_range_shift(1, 0, currow, 0, currow, maxcol);
                fix_marks(1, 0, currow, maxrow, 0, maxcol);
                insert_row(0);

            } else if (buf->pnext->value == 'c') {
                save_undo_range_shift(0, 1, 0, curcol, maxrow, curcol);
                fix_marks(0, 1, 0, maxrow, curcol, maxcol);
                insert_col(0);
            }
            end_undo_action();
            update();
            break;
            }
 
        case 'y':
            // yank row
            if ( bs == 2 && buf->pnext->value == 'r') {        
                yank_area(currow, 0, currow + cmd_multiplier, maxcol, 'r', cmd_multiplier + 1);
                if (cmd_multiplier > 0) cmd_multiplier = 0;

            // yank col
            } else if ( bs == 2 && buf->pnext->value == 'c') {
                yank_area(0, curcol, maxrow, curcol + cmd_multiplier, 'c', cmd_multiplier + 1);
                if (cmd_multiplier > 0) cmd_multiplier = 0;

            // yank cell
            } else if ( bs == 2 && buf->pnext->value == 'y' && is_range_selected() == -1) {
                yank_area(currow, curcol, currow, curcol, 'e', cmd_multiplier + 1);

            // yank range
            } else if ( bs == 1 && is_range_selected() != -1) {
                srange * r = get_selected_range();
                yank_area(r->tlrow, r->tlcol, r->brrow, r->brcol, 'a', cmd_multiplier + 1);
            }
            break;
               // paste cell below or left
               
        case 'p':
            paste_yanked_ents(0);
            update();
            break;

        // paste cell above or right
        case 'P':
            paste_yanked_ents(1);
            update();
            break;

        // select inner range - Vir
        case 'V':
            if (buf->value == 'V' && bs == 3 &&
            buf->pnext->value == 'i' && buf->pnext->pnext->value == 'r') {
                int tlrow = currow;
                int brrow = currow;
                int tlcol = curcol;
                int brcol = curcol;
                int * tlr = &tlrow;
                int * brr = &brrow;
                int * tlc = &tlcol;
                int * brc = &brcol;
                select_inner_range(tlr, tlc, brr, brc);
                start_visualmode(*tlr, *tlc, *brr, *brc);
            }
            break;
          
        // scroll
        case 'z':
            if ( bs != 2 ) break;
            int scroll = 0;

            switch (buf->pnext->value) {
                case 'l':
                    scroll_right(1);
                    //unselect_ranges();
                    break;

                case 'h':
                    scroll_left(1);
                    //unselect_ranges();
                    break;

                case 'H':
                    scroll = calc_offscr_sc_cols();
                    if (atoi(get_conf_value("half_page_scroll"))) scroll /= 2;
                    scroll_left(scroll);
                    //unselect_ranges();
                    break;

                case 'L':
                    scroll = calc_offscr_sc_cols();
                    if (atoi(get_conf_value("half_page_scroll"))) scroll /= 2;
                    scroll_right(scroll);
                    //unselect_ranges();
                    break;

                case 'm':
                    ;
                    int i = 0, c = 0, ancho = rescol;
                    offscr_sc_cols = 0;
 
                    for (i = 0; i < curcol; i++) {
                        for (c = i; c < curcol; c++) {
                            if (!col_hidden[c]) ancho += fwidth[c];
                            if (ancho >= (COLS - rescol)/ 2) {
                                ancho = rescol;
                                break;
                            } 
                        }
                        if (c == curcol) break;
                    }
                    offscr_sc_cols = i;
                    break;

                case 'z':
                case '.':
                case 't':
                case 'b':
                    if (buf->pnext->value == 'z' || buf->pnext->value == '.')
                        scroll = currow - offscr_sc_rows + LINES - RESROW - 2 - (LINES - RESROW - 2)/2; // zz
                    else if (buf->pnext->value == 't')
                        scroll = currow - offscr_sc_rows + 1;
                    else if (buf->pnext->value == 'b')
                        scroll = currow - offscr_sc_rows - LINES + RESROW + 2;

                    if (scroll > 0)
                        scroll_down(scroll);
//                    else if (scroll > offscr_sc_rows)
//                        scroll_up(-scroll);
                    else if (scroll < 0)
                        scroll_up(-scroll);
//                    else if (offscr_sc_rows > 0)
//                        scroll_up(offscr_sc_rows);
                    break;

            }
            update();
            break;
 
        // scroll up a line
        case ctl('y'):
            scroll_up(1);
            update();
            break;

        // scroll down a line
        case ctl('e'):
            scroll_down(1);
            update();
            break;

        // undo
        case 'u':
            do_undo();
            // sync_refs();
            update();
            break;

        // redo
        case ctl('r'):
            do_redo();
            // sync_refs();
            update();
            break;

        case '{': // left align
        case '}': // right align
        case '|': // center align
            {
            int p, r = currow, c = curcol, rf = currow, cf = curcol;
            struct srange * sr;
            if ( (p = is_range_selected()) != -1) {
                sr = get_range_by_pos(p);
                r = sr->tlrow;
                c = sr->tlcol;
                rf = sr->brrow;
                cf = sr->brcol;
            }
            if (any_locked_cells(r, c, rf, cf)) {
                error("Locked cells encountered. Nothing changed");           
                return;
            }
            create_undo_action();
            if (buf->value == '{')      sprintf(interp_line, "leftjustify %s", v_name(r, c));
            else if (buf->value == '}') sprintf(interp_line, "rightjustify %s", v_name(r, c));
            else if (buf->value == '|') sprintf(interp_line, "center %s", v_name(r, c));
            if (p != -1) sprintf(interp_line, "%s:%s", interp_line, v_name(rf, cf));
            copy_to_undostruct(r, c, rf, cf, 'd');
            send_to_interp(interp_line);
            copy_to_undostruct(r, c, rf, cf, 'a');
            end_undo_action();            
            cmd_multiplier = 0;
            update();
            break;
            }

        case ctl('l'):
            endwin();
            start_screen();
            clearok(stdscr, TRUE);
            update();
            flushinp();
            show_header(input_win);
            show_celldetails(input_win);
            wrefresh(input_win);
            update();
            break;

        case '@':
            EvalAll();
            update();
            break;
 
        // increase or decrease numeric value of cell or range
        case '-':
        case '+':
            {
            int r, c, tlrow = currow, tlcol = curcol, brrow = currow, brcol = curcol;
            if ( is_range_selected() != -1 ) {
                struct srange * sr = get_selected_range();
                tlrow = sr->tlrow;
                tlcol = sr->tlcol;
                brrow = sr->brrow;
                brcol = sr->brcol;
            }
            if (any_locked_cells(tlrow, tlcol, brrow, brcol)) {
                error("Locked cells encountered. Nothing changed");           
                return;
            }
            if (atoi(get_conf_value("numeric")) == 1) goto numeric;
            struct ent * p;
            create_undo_action();
            int arg = cmd_multiplier + 1;
            int mf = modflg; // keep original modflg
            for (r = tlrow; r <= brrow; r++) {
                for (c = tlcol; c <= brcol; c++) {
                    p = *ATBL(tbl, r, c);
                    if ( ! p )  {
                        continue;
                    } else if (p->expr && !(p->flags & is_strexpr)) {
                        //error("Can't increment / decrement a formula");
                        continue;
                    } else if (p->flags & is_valid) {
                        copy_to_undostruct(r, c, r, c, 'd');
                        p->v += buf->value == '+' ? (double) arg : - 1 * (double) arg;
                        copy_to_undostruct(r, c, r, c, 'a');
                        if (mf == modflg) modflg++; // increase just one time
                    }
                }
            }
            end_undo_action();
            if (atoi(get_conf_value("autocalc"))) EvalAll();
            cmd_multiplier = 0;
            update();
            }
            break;

        // input of numbers
        default:
        numeric:
            if ( (isdigit(buf->value) || buf->value == '-' || buf->value == '+') &&
                atoi(get_conf_value("numeric")) ) {
                insert_edit_submode='=';
                chg_mode(insert_edit_submode);
                inputline_pos = 0;
                ins_in_line(buf->value);
                show_header(input_win);
            }
    }
    return;
}
