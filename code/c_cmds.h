#include "d_player.h"

void C_InstallCommands (void);

#define CMD(a) void a (player_t *plyr, int argc, char **argv);

// am_map.c
CMD(Cmd_Togglemap)

// d_main.c
CMD(Cmd_Endgame)

// m_menu.c
CMD(Cmd_Menu_Main)
CMD(Cmd_Menu_Load)
CMD(Cmd_Menu_Save)
CMD(Cmd_Menu_Help)
CMD(Cmd_Quicksave)
CMD(Cmd_Quickload)
CMD(Cmd_Menu_Endgame)
CMD(Cmd_Menu_Quit)
CMD(Cmd_Menu_Game)
CMD(Cmd_Menu_Options)
CMD(Cmd_Menu_Player)
CMD(Cmd_Bumpgamma)
CMD(Cmd_ToggleMessages)

// m_options.c
CMD(Cmd_Sizedown)
CMD(Cmd_Sizeup)
CMD(Cmd_Menu_Video)
CMD(Cmd_Menu_Display)
CMD(Cmd_Menu_Keys)
CMD(Cmd_Menu_Gameplay)

// g_game.c
CMD(Cmd_Impulse)
CMD(Cmd_CenterView)
CMD(Cmd_Pause)
CMD(Cmd_WeapNext)
CMD(Cmd_WeapPrev)
CMD(Cmd_Stop)
CMD(Cmd_SpyNext)
CMD(Cmd_SpyPrev)
CMD(Cmd_Turn180)
CMD(Cmd_PlayDemo)
CMD(Cmd_TimeDemo)

// g_level.c
CMD(Cmd_Map)

// c_dispch.c
CMD(Cmd_Alias)
CMD(Cmd_Cmdlist)
CMD(Cmd_Key)

// c_bindng.c
CMD(Cmd_Bind)
CMD(Cmd_BindDefaults)
CMD(Cmd_Unbind)
CMD(Cmd_Unbindall)
CMD(Cmd_DoubleBind)
CMD(Cmd_UnDoubleBind)

// c_consol.c
void C_ToggleConsole (void);
CMD(Cmd_History)
CMD(Cmd_Clear)
CMD(Cmd_Echo)

// c_cvars.c
CMD(Cmd_Set)
CMD(Cmd_Get)
CMD(Cmd_Toggle)
CMD(Cmd_CvarList)

// c_cmds.c
CMD(Cmd_Give)
CMD(Cmd_God)
CMD(Cmd_Notarget)
CMD(Cmd_Noclip)
CMD(Cmd_Chase)
CMD(Cmd_ChangeMus)
CMD(Cmd_idmus)
CMD(Cmd_idclev)
CMD(Cmd_Gameversion)
CMD(Cmd_Exec)
CMD(Cmd_DumpHeap)
CMD(Cmd_Logfile)
CMD(Cmd_Limits)
CMD(Cmd_ChangeMap)
CMD(Cmd_Quit)
CMD(Cmd_Puke)
CMD(Cmd_Error)
CMD(Cmd_Dir)

// d_net.c
CMD(Cmd_Pings)

// p_inter.c
CMD(Cmd_Kill)

// v_video.c
CMD(Cmd_SetColor)
CMD(Cmd_Vid_SetMode)

// m_misc.c
CMD(Cmd_Screenshot)

// hu_stuff.c
CMD(Cmd_MessageMode)
CMD(Cmd_Say)
CMD(Cmd_MessageMode2)
CMD(Cmd_Say_Team)

// r_things.c
CMD(Cmd_Skins)

// s_sounds.c
CMD(Cmd_Soundlist)
CMD(Cmd_Soundlinks)

// z_zone.c
CMD(Cmd_Mem)

#undef CMD