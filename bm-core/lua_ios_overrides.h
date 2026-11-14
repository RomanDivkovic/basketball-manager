// iOS has no system(): Lua's os.execute reports failure instead
#define l_system(cmd) (-1)
