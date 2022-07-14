#include <yed/plugin.h>

typedef struct {
    yed_frame *frame;
    int        indx;
} frame_fixup;

static yed_plugin *Self;
static array_t     queue;
static int         command_indx;
static int         total_indx;
static FILE       *fp;

/* internal custom commands */
static void _frame_resize(int n_args, char **args);
static void _frame_set_pos(int n_args, char **args);

/* internal functions */
static void _save_layout_unload(yed_plugin *self);
static void _save_current_yed_layout(int n_args, char **args);
static void _search(yed_frame_tree *curr_frame_tree, int current_indx);
static void _calculate_moves(int start, int finish, int total);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    Self = self;

    yed_plugin_set_command(self, "save-current-yed-layout", _save_current_yed_layout);
    yed_plugin_set_command(self, "frame-resize-cmdl", _frame_resize);
    yed_plugin_set_command(self, "frame-set-pos", _frame_set_pos);

    if(yed_get_var("yed_layout_file") == NULL) {
        yed_set_var("yed_layout_file", "");
    }

    yed_plugin_set_unload_fn(self, _save_layout_unload);

    return 0;
}

static void _calculate_moves(int start, int finish, int total) {
    int prev_moves = 99999999;
    int next_moves = 99999999;

    if (start == finish) { return; }

    if (start < finish) {
        prev_moves = start + (total_indx - finish);
        next_moves = finish - start;
    } else {
        prev_moves = start - finish;
        next_moves = finish + (total_indx - start);
    }

    if (prev_moves >= next_moves) {
        for (int i = 0; i < next_moves; i++) {
/*             yed_log("frame-next\n"); */
            fprintf(fp, "frame-next\n");
        }
    } else {
        for (int i = 0; i < prev_moves; i++) {
/*             yed_log("frame-prev\n"); */
            fprintf(fp, "frame-prev\n");
        }
    }
}

static void _search(yed_frame_tree *curr_frame_tree, int current_indx) {
    if (curr_frame_tree->is_leaf) {
        if (curr_frame_tree->frame) {
            frame_fixup f_fixup;
            f_fixup.frame = curr_frame_tree->frame;
            f_fixup.indx  = current_indx;
            array_push(queue, f_fixup);
        }
        return;
    }

    _calculate_moves(command_indx, current_indx, total_indx);

    if (curr_frame_tree->split_kind == FTREE_VSPLIT) {
/*         yed_log("frame-vsplit\n"); */
        fprintf(fp, "frame-vsplit\n");
    } else {
/*         yed_log("frame-hsplit\n"); */
        fprintf(fp, "frame-hsplit\n");
    }

    command_indx  = total_indx;
    total_indx   += 1;

    if (curr_frame_tree              &&
        curr_frame_tree->child_trees &&
        curr_frame_tree->child_trees[1]) {

        _search(curr_frame_tree->child_trees[1], command_indx);
    }

    if (curr_frame_tree              &&
        curr_frame_tree->child_trees &&
        curr_frame_tree->child_trees[0]) {

        _search(curr_frame_tree->child_trees[0], current_indx);
    }
}

static void _save_current_yed_layout(int n_args, char **args) {
    yed_frame_tree **root;
    char             line[512];
    char             str[512];
    char             app[512];
    char            *path;
    frame_fixup     *f_fixup;
    int              row;
    int              col;

    if (!ys->options.no_init) {
        path = get_config_item_path("my_yed_layout.yedrc");
/*         yed_log("path: %s\n", path); */
        fp = fopen (path, "w");
        free(path);
    }

    if (fp == NULL) {
        return;
    }

/*     yed_log("start save\n"); */

    array_traverse(ys->frame_trees, root) {
        if ((*root) == yed_frame_tree_get_root(*root)) {
            command_indx = 0;
            total_indx   = 1;
            queue        = array_make(frame_fixup);

/*             yed_log("frame-new\n"); */
            fprintf(fp, "frame-new\n");
            _search(*root, 0);

            array_traverse(queue, f_fixup) {
                _calculate_moves(command_indx, (*f_fixup).indx, total_indx);
                command_indx = (*f_fixup).indx;

                if ((*f_fixup).frame->name) {
/*                     yed_log("frame-name %s\n", (*f_fixup).frame->name); */
                    fprintf(fp, "frame-name %s\n", (*f_fixup).frame->name);
                }

                if ((*f_fixup).frame->buffer) {
                    yed_log("buffer %s\n", (*f_fixup).frame->buffer->name);
                    fprintf(fp, "buffer %s\n", (*f_fixup).frame->buffer->name);
                }

/*                 yed_log("frame-move\n"); */
/*                 fprintf(fp, "frame-move\n"); */

                row = (*f_fixup).frame->bheight - ys->active_frame->bheight;
                col = (*f_fixup).frame->bwidth - ys->active_frame->bwidth;
                yed_log("frame-resize-cmdl %d %d\n", (*f_fixup).frame->bheight, (*f_fixup).frame->bwidth);
                yed_log("frame-resize-cmdl %f %f\n", (*f_fixup).frame->height_f, (*f_fixup).frame->width_f);
                fprintf(fp, "frame-resize-cmdl %f %f\n", (*f_fixup).frame->height_f, (*f_fixup).frame->width_f);
            }
            array_free(queue);
        }
    }

    fclose(fp);
}

static void _frame_resize(int n_args, char **args) {
    float fheight;
    float fwidth;
    int   cur_r;
    int   cur_c;
    int   row;
    int   col;

    if (n_args != 2) {
        yed_cerr("expected 2 argument, but got %d", n_args);
        return;
    }

    fheight = atof(args[0]);
    fwidth  = atof(args[1]);

    yed_log("%f %f\n", fheight, fwidth);

    if (ys->active_frame &&
        ys->active_frame->tree &&
        ys->active_frame->tree->parent) {

        cur_r = fheight * ys->term_rows;
        cur_c = fwidth  * ys->term_cols;

        yed_log("%d %d\n", cur_r, cur_c);

        row = cur_r - ys->active_frame->bheight;
        col = cur_c - ys->active_frame->bwidth;

        if (ys->active_frame->tree->parent->child_trees[0] == ys->active_frame->tree) {
            yed_resize_frame(ys->active_frame, row, col);
        } else {
            if (ys->active_frame->tree->parent->split_kind == FTREE_HSPLIT) {
                row *= -1;
                col  =  0;
            } else {
                row  =  0;
                col *= -1;
            }

            yed_resize_frame_tree(ys->active_frame->tree->parent->child_trees[0], row, col);
        }
    }
}

static void _frame_set_pos(int n_args, char **args) {

}

static void _save_layout_unload(yed_plugin *self) {
}
