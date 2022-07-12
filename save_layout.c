#include <yed/plugin.h>

static yed_plugin *Self;
static array_t     queue;
static int         command_indx;
static int         total_indx;

/* internal custom commands */
static void _frame_vsplit(int n_args, char **args);
static void _frame_hsplit(int n_args, char **args);
static void _frame_new_w_name(int n_args, char **args);
static void _frame_set_buffer(int n_args, char **args);
static void _frame_resize(int n_args, char **args);
static void _frame_set_pos(int n_args, char **args);

/* internal functions */
static void _save_layout_unload(yed_plugin *self);
static void _save_current_yed_layout(int n_args, char **args);
static void _load_yed_layout(int n_args, char **args);
static void _search(yed_frame_tree *curr_frame_tree, int current_indx);

int yed_plugin_boot(yed_plugin *self) {

    YED_PLUG_VERSION_CHECK();

    Self = self;

    yed_plugin_set_command(self, "save-current-yed-layout", _save_current_yed_layout);
    yed_plugin_set_command(self, "load-yed-layout", _load_yed_layout);

    if(yed_get_var("yed_layout_file") == NULL) {
        yed_set_var("yed_layout_file", "");
    }

    yed_plugin_set_unload_fn(self, _save_layout_unload);

    return 0;
}

static void _search(yed_frame_tree *curr_frame_tree, int current_indx) {
    int prev_moves = 99999999;
    int next_moves = 99999999;

    if (curr_frame_tree->is_leaf) {
        if (curr_frame_tree->frame) {
            if (curr_frame_tree->parent &&
                curr_frame_tree->parent->child_trees[0] == curr_frame_tree) {
                yed_log("add_leaf to queue F[%d]\n", current_indx);
            } else {
                yed_log("add_leaf to queue F[%d]\n", current_indx);
            }

/*             if (curr_frame_tree->frame->name) { */
/*                 yed_log("yed_frame_set_name(%s)\n", curr_frame_tree->frame->name); */
/*             } else { */
/*                 yed_log("Frame:None\n"); */
/*             } */
/*  */
/*             if (curr_frame_tree->frame->buffer && curr_frame_tree->frame->buffer->name) { */
/*                 yed_log("Buffer:%s\n", curr_frame_tree->frame->buffer->name); */
/*             } */
        }
        return;
    } else {
        if (command_indx != current_indx) {
            if (command_indx < current_indx) {
                prev_moves = command_indx + 1 + (current_indx - total_indx);
                next_moves = current_indx - command_indx;
            } else {
                prev_moves = command_indx - current_indx;
                next_moves = current_indx + 1 + (command_indx - total_indx);
            }

            if (prev_moves > next_moves) {
                for (int i = 0; i < next_moves; i++) {
                    yed_log("frame-next\n");
                }
            } else {
                for (int i = 0; i < next_moves; i++) {
                    yed_log("frame-prev\n");
                }
            }
        }

        if (curr_frame_tree->split_kind == FTREE_VSPLIT) {
            yed_log("frame-vsplit\n");
        } else {
            yed_log("frame-hsplit\n");
        }

        total_indx   += 1;
        command_indx  = total_indx;
    }

    if (curr_frame_tree              &&
        curr_frame_tree->child_trees &&
        curr_frame_tree->child_trees[1]) {

        _search(curr_frame_tree->child_trees[1], total_indx);
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
    FILE            *fp;

    if (!ys->options.no_init) {
        path = get_config_item_path("my_yed_layout.yedrc");
        fp = fopen (path, "w+");
        free(path);
    }

    if (fp == NULL) {
        return;
    }

    yed_log("start save\n");

    array_traverse(ys->frame_trees, root) {
        if ((*root) == yed_frame_tree_get_root(*root)) {
            command_indx = 0;
            total_indx   = 0;
            yed_log("frame-new\n");
            _search(*root, 0);
        }
    }

/*     queue = array_make(yed_frame_tree *); */
/*     yed_frame_tree *root; */
/*     root = yed_frame_tree_get_root(ys->active_frame->tree); */
/*     array_push(queue, root); */
/*     yed_frame_tree *curr_frame_tree; */
/*  */
/*     while (array_len(queue) != 0) { */
/*         curr_frame_tree = *(yed_frame_tree **)array_item(queue, 0); */
/*         array_delete(queue, 0); */
/*  */
/*         if (curr_frame_tree->is_leaf) { */
/*             if (curr_frame_tree->parent && */
/*                 curr_frame_tree->parent->child_trees[0] == curr_frame_tree) { */
/*                 yed_log("left\n"); */
/*             } else { */
/*                 yed_log("right\n"); */
/*             } */
/*  */
/*             if (curr_frame_tree->frame->name) { */
/*                 yed_log("yed_frame_set_name(%s)\n", curr_frame_tree->frame->name); */
/*             } else { */
/*                 yed_log("Frame:None\n"); */
/*             } */
/*  */
/*             if (curr_frame_tree->frame->buffer && curr_frame_tree->frame->buffer->name) { */
/*                 yed_log("Buffer:%s\n", curr_frame_tree->frame->buffer->name); */
/*             } */
/*  */
/*         } else { */
/*             if (curr_frame_tree->split_kind == FTREE_VSPLIT) { */
/*                 yed_log("frame-vsplit\n"); */
/*                 yed_log("frame-prev\n"); */
/*                 char tmp[512]; */
/*                 sprintf(tmp, "%d %d", curr_frame_tree->frame->height, curr_frame_tree->frame->width); */
/*                 yed_log("frame-resize %s\n", tmp); */
/*             } else { */
/*                 yed_log("frame-hsplit\n"); */
/*                 yed_log("frame-prev\n"); */
/*                 char tmp[512]; */
/*                 sprintf(tmp, "%d %d", curr_frame_tree->frame->height, curr_frame_tree->frame->width); */
/*                 yed_log("frame-resize %s\n", tmp); */
/*             } */
/*  */
/*             array_push(queue, curr_frame_tree->child_trees[0]); */
/*             array_push(queue, curr_frame_tree->child_trees[1]); */
/*         } */
/*     } */

    fclose(fp);
}

static void _load_yed_layout(int n_args, char **args) {
}

static void _save_layout_unload(yed_plugin *self) {
}
