#include "d_player.h"

void C_InstallCommands (void);

#define CMD(a) void a (player_t *plyr, int argc, char **argv);

// m_menu.c
CMD(Cmd_Sizedown)
CMD(Cmd_Sizeup)

// g_game.c
CMD(Cmd_Impulse)
CMD(Cmd_CenterView)
CMD(Cmd_Pause)
CMD(Cmd_Stop)

// c_dispatch.c
CMD(Cmd_Alias)
CMD(Cmd_Cmdlist)
CMD(Cmd_Key)

// c_bindings.c
CMD(Cmd_Bind)
CMD(Cmd_Unbind)
CMD(Cmd_Unbindall)

// c_console.c
void C_ToggleConsole (void);
CMD(Cmd_History)
CMD(Cmd_Clear)
CMD(Cmd_Echo)

// c_cvars.c
CMD(Cmd_Set)
CMD(Cmd_Get)
CMD(Cmd_Toggle)

// c_commands.c
CMD(Cmd_Give)
CMD(Cmd_God)
CMD(Cmd_Notarget)
CMD(Cmd_Noclip)
CMD(Cmd_idmus)
CMD(Cmd_idclev)
CMD(Cmd_idmypos)
CMD(Cmd_Gameversion)
CMD(Cmd_Exec)
CMD(Cmd_DumpHeap)
CMD(Cmd_Logfile)

// i_video.c
CMD(Cmd_Vid_DescribeDrivers)
CMD(Cmd_Vid_DescribeCurrentDriver)
CMD(Cmd_Vid_DescribeModes)
CMD(Cmd_Vid_DescribeCurrentMode)

// p_inter.c
CMD(Cmd_Kill)

// v_video.c
CMD(Cmd_Gamma)

#undef CMD