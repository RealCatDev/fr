#define NOB_IMPLEMENTATION
#include "./include/nob.h"

#include "./config.h"

const char *src_path = "./src/";
const char *build_path = "./build/";

const char *target = "./build/libfr.a";

#define CXXFLAGS "-Wall", "-Wpedantic", "-std=c++17"
#define INCLUDES "-I./include/", "-I"VULKAN_SDK_PATH"Include/", "-I"GLFW_PATH"include/", "-I"GLM_PATH"/"
#define LIBRARIES "-L"VULKAN_SDK_PATH"Lib/", "-lvulkan-1", "-L"GLFW_PATH"build/src/", "-lglfw3", "-lgdi32"

int compile() {
  nob_mkdir_if_not_exists(build_path);

  Nob_File_Paths paths = {0};
  nob_read_entire_dir(src_path, &paths);

  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "ar", "rcs", target);
  for (size_t i = 0; i < paths.count; ++i) {
    if (strlen(paths.items[i]) < 3) continue;
    Nob_String_Builder  path_sb = {0};
    nob_sb_append_cstr(&path_sb, src_path);
    nob_sb_append_cstr(&path_sb, paths.items[i]);
    nob_sb_append_null(&path_sb);

    Nob_String_Builder  out_sb = {0};
    nob_sb_append_cstr(&out_sb, build_path);
    nob_sb_append_cstr(&out_sb, paths.items[i]);
    nob_sb_append_cstr(&out_sb, ".o");
    nob_sb_append_null(&out_sb);

    Nob_Cmd obj_cmd = {0};
    nob_cmd_append(&obj_cmd, "g++", CXXFLAGS, INCLUDES, "-c", "-o", out_sb.items, path_sb.items);
    if (!nob_cmd_run_sync(obj_cmd)) return 1;
    nob_cmd_append(&cmd, out_sb.items);
  }

  if (!nob_cmd_run_sync(cmd)) return 1;

  nob_log(NOB_INFO, "successfully built library");

  return 0;
}

bool get_extension(const char *path, size_t *extPos) {
  size_t pathLen = strlen(path);
  for (size_t i = pathLen-1; i > 0; --i) {
    if (path[i] != '.') continue;
    *extPos = i;
    return true;
  }
  return false;
}

int compile_shader(const char *path) {
  if (!path) return 1;

  size_t extPos = 0;
  if (!get_extension(path, &extPos)) return 1;
  if (strcmp(path+extPos+1, "spv") == 0) return 0;

  char *output_path = malloc(extPos+1);
  memcpy(output_path, path, extPos);
  output_path[extPos] = '\0';

  Nob_String_Builder output_sb = {0};
  nob_sb_append_cstr(&output_sb, output_path);
  nob_sb_append_cstr(&output_sb, ".spv");
  nob_sb_append_null(&output_sb);

  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, VULKAN_SDK_PATH"Bin\\glslc", path, "-o", output_sb.items);
  if (!nob_cmd_run_sync(cmd)) return 1;

  nob_log(NOB_INFO, "Compiled %s -> %s\n", path, output_sb.items);

  return 0;
}

int example() {
  Nob_File_Paths paths = {0};
  if (!nob_read_entire_dir("./assets/shaders", &paths));

  for (size_t i = 0; i < paths.count; ++i) {
    if (strlen(paths.items[i]) < 3) continue;
    Nob_String_Builder sb = {0};
    nob_sb_append_cstr(&sb, "./assets/shaders/");
    nob_sb_append_cstr(&sb, paths.items[i]);
    nob_sb_append_null(&sb);
    compile_shader(sb.items);
    nob_sb_free(sb);
  }

  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "g++", CXXFLAGS, INCLUDES, "-o", "./build/example", "./example/main.cpp", "-L./build/", "-lfr", LIBRARIES);
  if (!nob_cmd_run_sync(cmd)) return 1;

  nob_log(NOB_INFO, "successfully built example");

  cmd.count = 0;
  nob_cmd_append(&cmd, "./build/example");
  if (!nob_cmd_run_sync(cmd)) return 1;

  return 0;
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char *const program = nob_shift_args(&argc, &argv);

  int result = 0;
  if ((result = compile()) != 0) return result;

  if (argc > 0) {
    const char *flag = nob_shift_args(&argc, &argv);
    if (strcmp(flag, "example") == 0) {
      result = example();
    } else {
      nob_log(NOB_ERROR, "unknown subcommand %s", flag);
    }
  }

  return result;
}