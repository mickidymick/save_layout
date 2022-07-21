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
static void _frame_open_command_line_buffers(int n_args, char **args);
static void _save_current_yed_layout(int n_args, char **args);
static void _save_current_yed_layout_local(int n_args, char **args);
static void _save_layout_clear_frame_tree(int n_args, char **args);

/* internal functions */
static void _save_layout_unload(yed_plugin *self);
static void _search(yed_frame_tree *curr_frame_tree, int current_indx);
static void _calculate_moves(int start, int finish, int total);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    Self = self;

    yed_plugin_set_command(self, "yed-clear-frame-trees", _save_layout_clear_frame_tree);
    yed_plugin_set_command(self, "save-current-yed-layout", _save_current_yed_layout);
    yed_plugin_set_command(self, "save-current-yed-layout-local", _save_current_yed_layout_local);
    yed_plugin_set_command(self, "save-layout-cmd", _save_layout_cmd);
    yed_plugin_set_command(self, "open-command-line-buffers", _frame_open_command_line_buffers);

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

            fprintf(fp, "save-layout-cmd \"yed-clear-frame-trees\"\n");

            fprintf(fp, "save-layout-cmd \"frame-new\"\n");

            fprintf(fp, "save-layout-cmd \"frame-tree-resize %f %f\"\n", (*root)->height, (*root)->width);

            fprintf(fp, "save-layout-cmd \"frame-tree-set-position %f %f\"\n", (*root)->top, (*root)->left);

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

                fprintf(fp, "save-layout-cmd \"frame-resize %f %f\"\n", (*f_fixup).frame->height_f, (*f_fixup).frame->width_f);
            }
            array_free(queue);
        }
    }

    fclose(fp);
}

static void _save_current_yed_layout_local(int n_args, char **args) {
    yed_frame_tree **root;
    char             line[512];
    char             str[512];
    char             app[512];
    char            *path;
    frame_fixup     *f_fixup;

    if (!ys->options.no_init) {
        path = strdup("my_yed_layout.yedrc");
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

            fprintf(fp, "save-layout-cmd \"yed-clear-frame-trees\"\n");

            fprintf(fp, "save-layout-cmd \"frame-new\"\n");

            fprintf(fp, "save-layout-cmd \"frame-tree-resize %f %f\"\n", (*root)->height, (*root)->width);

            fprintf(fp, "save-layout-cmd \"frame-tree-set-position %f %f\"\n", (*root)->top, (*root)->left);

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

                fprintf(fp, "save-layout-cmd \"frame-resize %f %f\"\n", (*f_fixup).frame->height_f, (*f_fixup).frame->width_f);
            }
            array_free(queue);
        }
    }

    fclose(fp);
}

static void _frame_open_command_line_buffers(int n_args, char **args) {
    yed_frame **frame_it;
    int i;

    if (array_len(ys->frames) <= 0){
        if (n_args >= 1) {
            YEXE("frame-new");
        }

        for (i = 0; i < n_args; i++) {
            YEXE("buffer", args[i]);
        }

        if (n_args >= 1) {
            YEXE("buffer", args[0]);
        }

        if (n_args > 1) {
            YEXE("frame-vsplit");
            YEXE("buffer", args[1]);
            YEXE("frame-prev");
        }
    } else {

        for (i = 0; i < n_args; i++) {
            if (!yed_get_buffer(args[i])) {
                YEXE("buffer-hidden", args[i]);
            }
        }

        for (i = 0; i < n_args; i++) {
            array_traverse(ys->frames, frame_it) {
                if ((*frame_it)->buffer) { continue; }

                yed_activate_frame((*frame_it));
                YEXE("buffer", args[i]);
                break;
            }
        }
    }
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

static void _save_layout_clear_frame_tree(int n_args, char **args) {
    yed_frame** fp;

    while(array_len(ys->frames) > 0) {
        fp = array_last(ys->frames);
        yed_delete_frame(*fp);
    }
}

static void _save_layout_unload(yed_plugin *self) {
    yed_set_var("yed-layout-reload", "yes");
}
