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
static void _save_layout_cmd(int n_args, char **args);
static void _frame_resize(int n_args, char **args);
static void _frame_tree_resize(int n_args, char **args);
static void _frame_tree_set_pos(int n_args, char **args);

/* internal functions */
static void _save_layout_unload(yed_plugin *self);
static void _save_current_yed_layout(int n_args, char **args);
static void _search(yed_frame_tree *curr_frame_tree, int current_indx);
static void _calculate_moves(int start, int finish, int total);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    Self = self;

    yed_plugin_set_command(self, "save-current-yed-layout", _save_current_yed_layout);
    yed_plugin_set_command(self, "save-layout-cmd", _save_layout_cmd);
    yed_plugin_set_command(self, "frame-resize-cmdl", _frame_resize);
    yed_plugin_set_command(self, "frame-tree-resize-cmdl", _frame_tree_resize);
    yed_plugin_set_command(self, "frame-tree-set-pos", _frame_tree_set_pos);

    if (yed_get_var("yed-layout-file") == NULL) {
        yed_set_var("yed-layout-file", "");
    }

    if (yed_get_var("yed-layout-reload") == NULL) {
        yed_set_var("yed-layout-reload", "no");
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
            fprintf(fp, "save-layout-cmd \"frame-next\"\n");
        }
    } else {
        for (int i = 0; i < prev_moves; i++) {
            fprintf(fp, "save-layout-cmd \"frame-prev\"\n");
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
        fprintf(fp, "save-layout-cmd \"frame-vsplit\"\n");
    } else {
        fprintf(fp, "save-layout-cmd \"frame-hsplit\"\n");
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

    if (!ys->options.no_init) {
        path = get_config_item_path("my_yed_layout.yedrc");
        fp = fopen (path, "w");
        free(path);
    }

    if (fp == NULL) {
        return;
    }

    array_traverse(ys->frame_trees, root) {
        if ((*root) == yed_frame_tree_get_root(*root)) {
            command_indx = 0;
            total_indx   = 1;
            queue        = array_make(frame_fixup);

            fprintf(fp, "save-layout-cmd \"frame-new\"\n");

            fprintf(fp, "save-layout-cmd \"frame-tree-resize-cmdl %f %f\"\n", (*root)->height, (*root)->width);

            fprintf(fp, "save-layout-cmd \"frame-tree-set-pos %f %f\"\n", (*root)->top, (*root)->left);

            _search(*root, 0);

            array_traverse(queue, f_fixup) {
                _calculate_moves(command_indx, (*f_fixup).indx, total_indx);
                command_indx = (*f_fixup).indx;

                if ((*f_fixup).frame->name) {
                    fprintf(fp, "save-layout-cmd \"frame-name %s\"\n", (*f_fixup).frame->name);
                }

                if ((*f_fixup).frame->buffer) {
                    fprintf(fp, "save-layout-cmd \"buffer %s\"\n", (*f_fixup).frame->buffer->name);
                }

                fprintf(fp, "save-layout-cmd \"frame-resize-cmdl %f %f\"\n", (*f_fixup).frame->height_f, (*f_fixup).frame->width_f);
            }
            array_free(queue);
        }
    }

    fclose(fp);
}

static void _save_layout_cmd(int n_args, char **args) {
    int     i;
    array_t split;

    if (n_args != 1) {
        yed_cerr("expected 1 argument, but got %d", n_args);
        return;
    }

    if(yed_var_is_truthy("yed-layout-reload")) {
        return;
    }

    for (i = 0; i < n_args; i += 1) {
        split = sh_split(args[i]);
        if (array_len(split)) {
            yed_execute_command_from_split(split);
        }
        free_string_array(split);
    }
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

    if (ys->active_frame &&
        ys->active_frame->tree &&
        ys->active_frame->tree->parent) {

        cur_r = fheight * ys->term_rows;
        cur_c = fwidth  * ys->term_cols;

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

static void _frame_tree_resize(int n_args, char **args) {
    yed_frame_tree *root;
    float fheight;
    float fwidth;
    int   root_r;
    int   root_c;
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

    if (ys->active_frame) {
        root  = yed_frame_tree_get_root(ys->active_frame->tree);
        cur_r  = fheight * ys->term_rows;
        cur_c  = fwidth  * ys->term_cols;
        root_r = root->height * ys->term_rows;
        root_c = root->width  * ys->term_cols;

        row = cur_r - root_r;
        col = cur_c - root_c;

        yed_resize_frame_tree(root, row, col);
    }
}

static void _frame_tree_set_pos(int n_args, char **args) {
    float ftop;
    float fleft;
    int   cur_r;
    int   cur_c;
    int   row;
    int   col;

    if (n_args != 2) {
        yed_cerr("expected 2 argument, but got %d", n_args);
        return;
    }

    ftop  = atof(args[0]);
    fleft = atof(args[1]);

    if (ys->active_frame) {
        cur_r = ftop  * ys->term_rows;
        cur_c = fleft * ys->term_cols;

        row = cur_r - ys->active_frame->top;
        col = cur_c - ys->active_frame->left;

        yed_move_frame(ys->active_frame, row, col);
    }
}

static void _save_layout_unload(yed_plugin *self) {
    yed_set_var("yed-layout-reload", "yes");
}
