
class LevelCompatibility : LevelPostProcessor
{
	protected void Apply(Name checksum, String mapname)
	{
		switch (checksum)
		{			
			case 'none':
				return;
				
			case '9BC9E12781903D7C2D5697A5E0AEFD6F': // HACX.WAD map05 from 21.10.2010
			case '9527DD0809FDA39CCFC316A21D135783': // HACX.WAD map05 from 20.10.2010
			{
				// fix non-functional self-referencing sector hack.
				for(int i = 578; i < 582; i++) 
					SetLineSectorRef(i, Line.back, 91);
				
				for(int i = 707; i < 714; i++)
					SetLineSectorRef(i, Line.back, 91);

				SetLineSectorRef(736, Line.front, 91);
				SetLineSectorRef(659, Line.front, 91);
				SetLineSpecial(659, Transfer_Heights, 60);
				break;
			}
			
		
			case 'E2B5D1400279335811C1C1C0B437D9C8': // Deathknights of the Dark Citadel, map54
			{
				// This map has two gear boxes which are flagged for player cross
				// activation instead of the proper player uses activation.
				SetLineActivation(943, SPAC_Use);
				SetLineActivation(963, SPAC_Use);
				break;
			}
			
			case '3F249EDD62A3A08F53A6C53CB4C7ABE5': // Artica 3 map01
			{
				ClearLineSpecial(66);
				break;
			}
			
			case 'F481922F4881F74760F3C0437FD5EDD0': // Community Chest 3 map03
			{
				// I have no idea how this conveyor belt setup manages to work under Boom.
				// Set the sector the voodoo doll ends up standing on when sectors tagged
				// 1 are raised so that the voodoo doll will be carried.
				SetLineSpecial(3559, Sector_CopyScroller, 17, 6);
				break;
			}

			case '5B862477519B21B30059A466F2FF6460': // Khorus, map08
			{
				// This map uses a voodoo conveyor with slanted walls to shunt the
				// voodoo doll into side areas. For some reason, this voodoo doll
				// is unable to slide on them, because the slide calculation gets
				// them slightly inside the walls and thinks they are stuck. I could
				// not reproduce this with the real player, which is what has me
				// stumped. So, help them out be attaching some ThrustThing specials
				// to the walls.
				SetLineSpecial(443, ThrustThing, 96, 4);
				SetLineFlags(443, Line.ML_REPEAT_SPECIAL);
				SetLineActivation(443, SPAC_Push);
				SetLineSpecial(455, ThrustThing, 96, 4);
				SetLineFlags(455, Line.ML_REPEAT_SPECIAL);
				SetLineActivation(455, SPAC_Push);
				break;
			}

			case '3D1E36E50F5A8D289E15433941605224': // Master Levels, catwalk.wad
			{
				// make it impossible to open door to 1-way bridge before getting red key
				ClearSectorTags(35);
				AddSectorTag(35, 15);
				for(int i=605; i<609;i++)
				{
					SetLineActivation(i, SPAC_Cross);
					SetLineSpecial(i, Door_Open, 15, 64);
				}
				break;
			}
			
			case '3B9CAA02952F405269353FAAD8F8EC33': // Master Levels, nessus.wad
			{
				// move secret sector from too-thin doorframe to BFG closet
				SetSectorSpecial(211, 0);
				SetSectorSpecial(212, 1024);
				// lower floor a bit so secret sector can be entered fully
				OffsetSectorPlane(212, Sector.floor, -16);
				// make secret door stay open so you can't get locked in closet
				SetLineSpecial(1008, Door_Open, 0, 64);
				break;
			}

			case '7ED9800213C00D6E7FB98652AB48B3DE': // Ultimate Simplicity, map04
			{
				// Add missing map spots on easy and medium skills
				// Demons will teleport into starting room making 100% kills possible
				SetThingSkills(31, 31);
				SetThingSkills(32, 31);
				break;
			}
  
			case '1891E029994B023910CFE0B3209C3CDB': // Ultimate Simplicity, map07
			{
				// It is possible to get stuck on skill 0 or 1 when no shots have been fired
				// after sector 17 became accessible and before entering famous mancubus room.
				// Monsters from the mentioned sector won't be alerted and so 
				// they won't teleport into the battle. ACS will wait forever for their deaths.
				SetLineSpecial(397, NoiseAlert);
				SetLineSpecial(411, NoiseAlert);
				break;
			}

			case 'F0E6F30F57B0425F17E43600AA813E80': // Ultimate Simplicity, map11
			{
				// If door (sector #309) is closed it cannot be open again
				// from one side potentially blocking level progression
				ClearLineSpecial(2445);
				break;
			}

			case '952CC8D03572E17BA550B01B366EFBB9': // Cheogsh map01
			{
				// make the blue key spawn above the 3D floor
				SetThingZ(918, 296);
				break;
			}

			case 'D62DCA9EC226DE49108D5DD9271F7631': // Cheogsh 2 map04
			{
				// Stuff in megasphere cage is positioned too low
				for(int i=1640; i<=1649; i++)
				{
					SetThingZ(i, 528);
				}
				break;
			}
			
			case '0F898F0688AECD42F2CD102FAE06F271': // TNT: Evilution MAP07
			{
				// Dropping onto the outdoor lava now also raises the
				// triangle sectors.
				SetLineSpecial(320, Floor_RaiseToNearest, 16, 32);
				SetLineActivation(320, SPAC_Cross);
				SetLineSpecial(959, Floor_RaiseToNearest, 16, 32);
				SetLineActivation(959, SPAC_Cross);
				SetLineSpecial(960, Floor_RaiseToNearest, 16, 32);
				SetLineActivation(960, SPAC_Cross);
				// Dropping into the holes themselves raises sectors
				for(int i=0; i<9; i++)
				{
					SetLineSpecial(999+i, Floor_RaiseToNearest, 16, 32);
					SetLineActivation(999+i, SPAC_Cross);
				}
				break;
			}
			
			case '1E785E841A5247B6223C042EC712EBB3': // TNT: Evilution MAP08
			{
				// Missing texture when lowering sector to red keycard
				SetWallTexture(480, Line.back, Side.bottom, "METAL2");
				break;
			}

			case 'DFC18B92BF3E8142B8684ECD8BD2EF06': // TNT: Evilution map15
			{
				// raise up sector with its counterpart so 100% kills becomes possible
				AddSectorTag(330, 11);
				break;
			}
			
			case '55C8DA7B531AE47014AD73FFF4687A36': // TNT: Evilution MAP21
			{
				// Missing texture that is most likely unintentional
				SetWallTexture(1138, Line.front, Side.top, "PANEL4");
				break;
			}

			case '2C4A3356C5EB3526D2C72A4AA4B18A36': // TNT: Evilution map29
			{
				// remove mancubus who always gets stuck in teleport tunnel, preventing
				// 100% kills on HMP
				SetThingFlags(405, 0);
				break;
			}

			case 'A53AE580A4AF2B5D0B0893F86914781E': // TNT: Evilution map31
			case '55192065F7FAA7D161767145DA008293': // TNT Anthology/GOG MAP31
			{
				// The famous missing yellow key...
				SetThingFlags(470, 2016);
				// Fix textures on the two switches that rise from the floor in the eastern area
				for (int i = 0; i < 8; i++)
				{
					SetLineFlags(1676 + i, 0, Line.ML_DONTPEGBOTTOM);
				}
				break;
			}

			case 'D99AD22FF21A41B4EECDB3A7C803D75E': // TNT: Evilution map32
			{
				// door can close permanently; make switch that opens it repeatable
				SetLineFlags(872, Line.ML_REPEAT_SPECIAL);
				// switch should only open way to red key, don't lower bars yet,
				// instead make line just before red key open bars
				ClearSectorTags(197);
				AddSectorTag(197, 8);
				SetLineSpecial(1279, Floor_LowerToLowest, 8, 32);
				SetLineActivation(1240, SPAC_Cross);
				SetLineSpecial(1240, Floor_LowerToLowest, 38, 32);
				break;
			}
			
			case '56D4060662C290791822EDB7225273B7': // Plutonia Experiment MAP08
			{
				// Raising ceiling causing a HOM.
				OffsetSectorPlane(130, Sector.ceiling, 16);
				break;
			}
			
			case '25A16C30CD10157382C01E5DD9C6604B': // Plutonia Experiment MAP10
			{
				// Missing textures
				SetWallTexture(548, Line.front, Side.bottom, "METAL");
				SetSectorTexture(71, Sector.ceiling, "FLOOR7_1");
				SetWallTexture(1010, Line.front, Side.top, "GSTONE1");
				break;
			}

			case '1EF7DEAECA03DE03BF363A3193757B5C': // PLUTONIA.wad map11
			{
				SetLineSectorRef(41, Line.back, 6);
				break;
			}
			
			case 'B02ACA32AE9630042543037BA630CDE9': // Plutonia Experiment MAP13
			{
				// Textures on wrong side at level start.
				SetWallTexture(107, Line.back, Side.top, "A-BROWN1");
				SetWallTexture(119, Line.back, Side.top, "A-BROWN1");
				break;
			}
			
			case 'ECA0559E85EFFB6966ECB8DE01E3A35B': // Plutonia Experiment MAP16
			{
				// Have it so that the slime pit at the end of the level can
				// actually kill the player.
				SetSectorSpecial(95, 768);
				SetSectorSpecial(96, 768);
				break;
			}
			
			case '9D84B423D8FD28553DDE23B55F97CF4A': // Plutonia Experiment MAP25
			{
				// Missing texture at level exit.
				SetWallTexture(1152, Line.front, Side.top, "A-BROCK2");
				break;
			}
			
			case 'ABC4EB5A1535ECCD0061AD14F3547908': // Plutonia Experiment, map26
			{
				SetSectorSpecial(156, 0);
				// Missing textures
				SetWallTexture(322, Line.front, Side.top, "A-MUD");
				SetWallTexture(323, Line.front, Side.top, "A-MUD");
				break;
			}
			
			case 'A2634C462717328CC1AD81E81EE77B08': // Plutonia Experiment MAP28
			{
				// Missing textures
				SetWallTexture(675, Line.front, Side.bottom, "BRICK10");
				SetWallTexture(676, Line.front, Side.bottom, "BRICK10");
				SetWallTexture(834, Line.front, Side.bottom, "WOOD8");
				SetWallTexture(835, Line.front, Side.bottom, "WOOD8");
				SetWallTexture(2460, Line.front, Side.top, "BIGDOOR7");
				SetWallTexture(2496, Line.front, Side.top, "BRICK10");
				
				// Allow a player to leave the room in deathmatch without
				// needing another player to activate a switch.
				SetLineSpecial(1033, Floor_LowerToLowest, 10, 8);
				SetLineActivation(1033, SPAC_Use);
				break;
			}
			
			case '850AC6D62F0AC57A4DD7EBC2689AC38E': // Plutonia Experiment MAP29
			{
				// Texture applied on bottom instead of top
				SetWallTexture(2842, Line.front, Side.top, "A-BROCK2");
				break;
			}
			
			case '279BB50468FE9F5B36C6D821E4902369': // Plutonia Experiment map30
			{
				// Missing texture and unpegged gate texture during boss reveal
				SetWallTexture(730, Line.front, Side.bottom, "ROCKRED1");
				SetLineFlags(730, 0, Line.ML_DONTPEGBOTTOM);
				// flag items in deathmatch-only area correctly so that 100% items
				// are possible in solo
				SetThingFlags(250, 17);
				SetThingFlags(251, 17);
				SetThingFlags(252, 17);
				SetThingFlags(253, 17);
				SetThingFlags(254, 17);
				SetThingFlags(206, 17);
				break;
			}
			
			case 'D5F64E02679A81B82006AF34A6A8EAC3': // Plutonia Experiment MAP32
			{
				// Missing textures
				TextureID mosrok = TexMan.CheckForTexture("A-MOSROK", TexMan.Type_Wall);
				for(int i=0; i<4; i++)
				{
					SetWallTextureID(569+i, Line.front, Side.top, MOSROK);
				}
				SetWallTexture(805, Line.front, Side.top, "A-BRICK3");
				break;
			}

			case '3B68019EE3154C284B90F0CAEDBD8D8A': // Plutonia 2 MAP05
			{
				// Missing texture
				TextureID step1 = TexMan.CheckForTexture("STEP1", TexMan.Type_Wall);
				SetWallTextureID(1525, Line.front, Side.bottom, step1);
				break;
			}

			case 'FB613B36589FFB09AA2C03633A7D13F4': // Plutonia 2 MAP20
			{
				// Remove the pain elementals stuck in the closet boxes that cannot teleport.
				SetThingFlags(758,0);
				SetThingFlags(759,0);
				SetThingFlags(764,0);
				SetThingFlags(765,0);
				break;
			}

			case 'A3165C53F9BF0B7D80CDB14665A349EB': // Plutonia 2 MAP23
			{
				// Arch-vile in outdoor secret area sometimes don't spawn if revenants
				// block its one-time teleport. Make this teleport repeatable to ensure
				// maxkills are always possible.
				SetLineFlags(756, Line.ML_REPEAT_SPECIAL);
				break;
			}

			case '9191658A6705B131AB005948E2FDFCE1': // Plutonia 2 MAP24
			{
				// Fix improperly pegged blue door
				for(int i = 957; i <= 958; i++)
				{
					SetLineFlags(i, 0, Line.ML_DONTPEGTOP);
					level.lines[i].sidedef[0].SetTextureYOffset(Side.top,-24);
				}
				break;
			}

			case 'EF251B8F36DE709901B0D32A97F341D7': // Plutonia 2 MAP27
			{
				// Remove the monsters stuck in the closet boxes that cannot teleport.

				// Top row, 2nd from left
				SetThingFlags(156,0);
				SetThingFlags(210,0);
				SetThingFlags(211,0);

				// 2nd row, 2nd-5th from left
				for(int i = 242; i <= 249; i++)
					SetThingFlags(i,0);

				// 3rd row, rightmost box
				SetThingFlags(260,0);
				SetThingFlags(261,0);
				SetThingFlags(266,0);
				SetThingFlags(271,0);
				SetThingFlags(272,0);
				SetThingFlags(277,0);
				SetThingFlags(278,0);
				SetThingFlags(283,0);

				break;
			}

			case '4CB7AAC5C43CF32BDF05FD36481C1D9F': // Plutonia: Revisited map27
			{
				SetLineSpecial(1214, Plat_DownWaitUpStayLip, 20, 64, 150);
				SetLineSpecial(1215, Plat_DownWaitUpStayLip, 20, 64, 150);
				SetLineSpecial(1216, Plat_DownWaitUpStayLip, 20, 64, 150);
				SetLineSpecial(1217, Plat_DownWaitUpStayLip, 20, 64, 150);
				SetLineSpecial(1227, Plat_DownWaitUpStayLip, 20, 64, 150);
				break;
			}

			case '5B26545FF21B051CA06D389CE535684C': // doom.wad e1m4
			{
				// missing textures	
				SetWallTexture(693, Line.back, Side.top, "BROWN1");
				// fix HOM errors with sectors too low
				OffsetSectorPlane(9,  Sector.floor, 8);
				OffsetSectorPlane(105, Sector.floor, 8);
				OffsetSectorPlane(132, Sector.floor, 8);
				OffsetSectorPlane(137, Sector.floor, 8);
				break;
			}
			
			case 'A24FE135D5B6FD427FE27BEF89717A65': // doom.wad e2m2
			{
				// missing textures
				SetWallTexture(947, Line.back, Side.top, "BROWN1");
				SetWallTexture(1596, Line.back, Side.top, "WOOD1");
				break;
			}
			
			case '1BC04D646B32D3A3E411DAF3C1A38FF8': // doom.wad e2m4
			{
				// missing textures
				SetWallTexture(551, Line.back, Side.top, "PIPE4");
				SetWallTexture(865, Line.back, Side.bottom, "STEP5");
				SetWallTexture(1062, Line.front, Side.top, "GSTVINE1");
				SetWallTexture(1071, Line.front, Side.top, "MARBLE1");
				// Door requiring yellow keycard can only be opened once,
				// change other side to one-shot Door_Open
				SetLineSpecial(165, Door_Open, 0, 16);
				SetLineFlags(165, 0, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '99C580AD8FABE923CAB485CB7F3C5E5D': // doom.wad e2m5
			{
				// missing textures
				SetWallTexture(590, Line.back, Side.top, "GRAYBIG");
				SetWallTexture(590, Line.front, Side.bottom, "BROWN1");
				SetWallTexture(1027, Line.back, Side.top, "SP_HOT1");
				// Replace tag for eastern secret at level start to not
				// cause any action to the two-Baron trap
				ClearSectorTags(127);
				AddSectorTag(127, 100);
				SetLineSpecial(382, Door_Open, 100, 16);
				SetLineSpecial(388, Door_Open, 100, 16);
				break;
			}
			
			case '3838AB29292587A7EE3CA71E7040868D': // doom.wad e2m6
			{
				// missing texture
				SetWallTexture(1091, Line.back, Side.top, "compspan");
				break;
			}
			
			case '8590F489879870C098CD7029C3187159': // doom.wad e2m7
			{
				// missing texture
				SetWallTexture(1286, Line.front, Side.bottom, "SHAWN2");
				break;
			}
			
			case '8A6399FAAA2E68649D4E4B16642074BE': // doom.wad e2m9
			{
				// missing textures
				SetWallTexture(121, Line.back, Side.top, "SW1LION");
				SetWallTexture(123, Line.back, Side.top, "GSTONE1");
				SetWallTexture(140, Line.back, Side.top, "GSTONE1");
				break;
			}
			
			case 'BBDC4253AE277DA5FCE2F19561627496': // Doom E3M2
			{
				// Switch at index finger repeatable in case of being stuck
				SetLineFlags(368, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '2B65CB046EA40D2E44576949381769CA': // Commercial Doom e3m4
			{
				// This line is erroneously specified as Door_Raise that monsters
				// can operate. If they do, they block you off from half the map. Change
				// this into a one-shot Door_Open so that it won't close.
				SetLineSpecial(1069, Door_Open, 0, 16);
				SetLineFlags(1069, 0, Line.ML_REPEAT_SPECIAL);
				// Fix HOM error from AASTINKY texture
				SetWallTexture(470, Line.front, Side.top, "BIGDOOR2");
				break;
			}
			
			case '100106C75157B7DECB0DCAD2A59C1919': // Doom E3M5
			{
				// Replace AASTINKY textures
				SetWallTexture(833, Line.front, Side.mid, "FIREWALL");
				SetWallTexture(834, Line.front, Side.mid, "FIREWALL");
				SetWallTexture(836, Line.front, Side.mid, "FIREWALL");
				SetWallTexture(839, Line.front, Side.mid, "FIREWALL");
				// Missing textures at the door leading to the BFG
				SetWallTexture(1329, Line.back, Side.bottom, "METAL");
				SetWallTexture(1330, Line.back, Side.bottom, "METAL");
				break;
			}
			
			case '5AC51CA9F1B57D4538049422A5E37291': // doom.wad e3m7
			{
				// missing textures
				SetWallTexture(901, Line.back, Side.bottom, "GSTONE1");
				SetWallTexture(971, Line.back, Side.top, "SP_HOT1");
				break;
			}
			
			case 'FE97DCB9E6235FB3C52AE7C143160D73': // Doom E3M9
			{
				// Missing textures
				SetWallTexture(102, Line.back, Side.bottom, "STONE3");
				SetWallTexture(445, Line.back, Side.bottom, "GRAY4");
				// First door cannot be opened from inside and can stop you
				// from finishing the level if somehow locked in.
				SetLineSpecial(24, Door_Open, 0, 16);
				SetLineActivation(24, SPAC_Use);
				SetLineFlags(24, Line.ML_REPEAT_SPECIAL);
				// Door that requires blue skull can only be opened once,
				// make it repeatable in case of being locked
				SetLineFlags(194, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case 'DA0C8281AC70EEC31127C228BCD7FE2C': // doom.wad e4m1
			{
				// missing textures
				TextureID support3 = TexMan.CheckForTexture("SUPPORT3", TexMan.Type_Wall);
				for(int i=0; i<4; i++)
				{
					SetWallTextureID(252+i, Line.back, Side.top, SUPPORT3);
				}
				SetWallTexture(322, Line.back, Side.bottom, "GSTONE1");
				SetWallTexture(470, Line.front, Side.top, "GSTONE1");
				break;
			}
			
			case '771092812F38236C9DF2CB06B2D6B24F': // Ultimate Doom E4M2
			{
				// Missing texture
				SetWallTexture(165, Line.back, Side.top, "WOOD5");
				break;
			}
			
			case 'F6EE16F770AD309D608EA0B1F1E249FC': // Ultimate Doom, e4m3
			{
				// Remove unreachable secrets
				SetSectorSpecial(124, 0);
				SetSectorSpecial(125, 0);
				// clear staircase to secret area
				SetSectorSpecial(127, 0);
				SetSectorSpecial(128, 0);
				SetSectorSpecial(129, 0);
				SetSectorSpecial(130, 0);
				SetSectorSpecial(131, 0);
				SetSectorSpecial(132, 0);
				SetSectorSpecial(133, 0);
				SetSectorSpecial(134, 0);
				SetSectorSpecial(136, 0);
				SetSectorSpecial(137, 0);
				SetSectorSpecial(138, 0);
				SetSectorSpecial(147, 0);
				SetSectorSpecial(148, 0);
				SetSectorSpecial(149, 0);
				SetSectorSpecial(150, 0);
				SetSectorSpecial(151, 0);
				SetSectorSpecial(152, 0);
				SetSectorSpecial(155, 0);
				// Stuck Imp
				SetThingXY(69, -656, -1696);
				// One line special at the northern lifts is incorrect, change
				// to a repeatable line you can walk over to lower lift
				SetLineSpecial(46, Plat_DownWaitUpStayLip, 1, 32, 105, 0);
				SetLineActivation(46, SPAC_AnyCross);
				SetLineFlags(46, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case 'AAECADD4D97970AFF702D86FAFAC7D17': // doom.wad e4m4
			{
				// missing textures
				TextureID brownhug = TexMan.CheckForTexture("BROWNHUG", TexMan.Type_Wall);
				SetWallTextureID(427, Line.back, Side.top, BROWNHUG);
				SetWallTextureID(558, Line.back, Side.top, BROWNHUG);
				SetWallTextureID(567, Line.front, Side.top, BROWNHUG);
				SetWallTextureID(572, Line.front, Side.top, BROWNHUG);
				break;
			}
			
			case 'C2E09AB0BDD03925305A48AE935B71CA': // Ultimate Doom E4M5
			{
				// Missing textures
				SetWallTexture(19, Line.back, Side.bottom, "GSTONE1");
				SetWallTexture(109, Line.back, Side.top, "GSTONE1");
				SetWallTexture(711, Line.back, Side.bottom, "FIRELAV2");
				SetWallTexture(713, Line.back, Side.bottom, "FIRELAV2");
				// Lower ceiling on teleporter to fix HOMs.
				OffsetSectorPlane(35, Sector.ceiling, -24);
				break;
			}
			
			case 'CBBFF61A8C231DFFC8E8A2A2BAEB77FF': // Ultimate Doom E4M6
			{
				// Textures on wrong side at Yellow Skull room.
				SetWallTexture(475, Line.back, Side.top, "MARBLE2");
				SetWallTexture(476, Line.back, Side.top, "MARBLE2");
				SetWallTexture(479, Line.back, Side.top, "MARBLE2");
				SetWallTexture(480, Line.back, Side.top, "MARBLE2");
				SetWallTexture(481, Line.back, Side.top, "MARBLE2");
				SetWallTexture(482, Line.back, Side.top, "MARBLE2");
				break;
			}
			
			case '94D4C869A0C02EF4F7375022B36AAE45': // Ultimate Doom, e4m7
			{
				// Remove unreachable secrets
				SetSectorSpecial(263, 0);
				SetSectorSpecial(264, 0);
				break;
			}
			
			case '2DC939E508AB8EB68AF79D5B60568711': // Ultimate Doom E4M8
			{
				// Missing texture
				SetWallTexture(425, Line.front, Side.mid, "SP_HOT1");
				break;
			}
			
			case 'AB24AE6E2CB13CBDD04600A4D37F9189':   // doom2.wad map02
			case '1EC0AF1E3985650F0C9000319C599D0C':  // doom2bfg.wad map02
			{
				// Missing textures
				TextureID stone4 = TexMan.CheckForTexture("STONE4", TexMan.Type_Wall);
				SetWallTextureID(327, Line.front, Side.bottom, stone4);
				SetWallTextureID(328, Line.front, Side.bottom, stone4);
				SetWallTextureID(338, Line.front, Side.bottom, stone4);
				SetWallTextureID(339, Line.front, Side.bottom, stone4);
				break;
			}

			case 'CEC791136A83EEC4B91D39718BDF9D82': // doom2.wad map04
			{
				// missing textures
				SetWallTexture(456, Line.back, Side.top, "SUPPORT3");
				TextureID stone = TexMan.CheckForTexture("STONE", TexMan.Type_Wall);
				SetWallTextureID(108, Line.front, Side.top, STONE);
				SetWallTextureID(109, Line.front, Side.top, STONE);
				SetWallTextureID(110, Line.front, Side.top, STONE);
				SetWallTextureID(111, Line.front, Side.top, STONE);
				SetWallTextureID(127, Line.front, Side.top, STONE);
				SetWallTextureID(128, Line.front, Side.top, STONE);
				// remove erroneous blue keycard pickup ambush sector tags
				// (nearby viewing windows, and the lights)
				ClearSectorTags(19);
				ClearSectorTags(20);
				ClearSectorTags(23);
				ClearSectorTags(28);
				ClearSectorTags(33);
				ClearSectorTags(34);
				ClearSectorTags(83);
				ClearSectorTags(85);
				// Visually align floors between crushers
				SetLineSpecial(3, Transfer_Heights, 14, 6);
				break;
			}
			
			case '9E061AD7FBCD7FAD968C976CB4AA3B9D': // doom2.wad map05
			{
				// fix bug with opening westmost door in door hallway
				// incorrect sector tagging - see doomwiki.org for more info
				ClearSectorTags(4);
				ClearSectorTags(153);
				// Missing textures
				SetWallTexture(489, Line.back, Side.top, "SUPPORT3");
				SetWallTexture(560, Line.back, Side.top, "SUPPORT3");
				break;
			}
			
			case '291F24417FB3DD411339AE82EF9B3597': // Doom II MAP07
			{
				// Missing texture at BFG deathmatch spawn.
				SetWallTexture(168, Line.back, Side.bottom, "BRONZE3");
				SetLineFlags(168, 0, Line.ML_DONTPEGBOTTOM);
				break;
			}
			
			case '66C46385EB1A23D60839D1532522076B':  // doom2.wad map08
			{
				// Missing texture
				SetWallTexture(101, Line.back, Side.top, "BRICK7");
				break;
			}
			
			case '6C620F43705BEC0ABBABBF46AC3E62D2': // Doom II MAP10
			{
				// Allow player to leave exit room
				SetLineSpecial(786, Door_Raise, 0, 16, 150, 0);
				SetLineActivation(786, SPAC_Use);
				SetLineFlags(786, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '1AF4DEC2627360A55B3EB397BC15C39D': // Doom II MAP12
			{
				// Missing texture
				SetWallTexture(648, Line.back, Side.bottom, "PIPES");
				// Remove erroneous tag for teleporter at the south building
				ClearSectorTags(149);
				break;
			}
			
			case 'FBA6547B9FD44E95671A923A066E516F': // Doom II MAP13
			{
				// Missing texture
				SetWallTexture(622, Line.back, Side.top, "BROWNGRN");
				break;
			}
			
			case '5BDA34DA60C0530794CC1EA2DA017976': // doom2.wad map14
			{
				// missing textures
				SetWallTexture(429, Line.front, Side.top, "BSTONE2");
				SetWallTexture(430, Line.front, Side.top, "BSTONE2");
				SetWallTexture(531, Line.front, Side.top, "BSTONE1");
				SetWallTexture(1259, Line.back, Side.top, "BSTONE2");
				SetWallTexture(1305, Line.back, Side.top, "BSTONE2");
				
				TextureID bstone2 = TexMan.CheckForTexture("BSTONE2", TexMan.Type_Wall);
				for(int i=0; i<3; i++)
				{
					SetWallTextureID(607+i, Line.back, Side.top, BSTONE2);
				}
				
				TextureID tanrock5 = TexMan.CheckForTexture("TANROCK5", TexMan.Type_Wall);
				for(int i=0; i<7; i++)
				{
					SetWallTextureID(786+i, Line.back, Side.top, TANROCK5);
				}
				
				TextureID bstone1 = TexMan.CheckForTexture("BSTONE1", TexMan.Type_Wall);
				for(int i=0; i<3; i++)
				{
					SetWallTextureID(1133+i, Line.back, Side.top, BSTONE1);
					SetWallTextureID(1137+i, Line.back, Side.top, BSTONE1);
					SetWallTextureID(1140+i, Line.back, Side.top, BSTONE1);
				}
				
				// Raise floor between lifts to correspond with others.
				OffsetSectorPlane(106, Sector.floor, 16);
				break;
			}
			
			case '1A540BA717BF9EC85F8522594C352F2A': // Doom II, map15
			{
				// Remove unreachable secret
				SetSectorSpecial(147, 0);
				// Missing textures
				SetWallTexture(94, Line.back, Side.top, "METAL");
				SetWallTexture(95, Line.back, Side.top, "METAL");
				SetWallTexture(989, Line.back, Side.top, "BRICK10");
				break;
			}
			
			case '6B60F37B91309DFF1CDF02E5E476210D': // Doom II MAP16
			{
				// Missing textures
				SetWallTexture(162, Line.back, Side.top, "BRICK6");
				SetWallTexture(303, Line.front, Side.top, "STUCCO");
				SetWallTexture(304, Line.front, Side.top, "STUCCO");
				break;
			}
			
			case 'E1CFD5C6E60C3B6C30F8B95FC287E9FE': // Doom II MAP17
			{
				// Missing texture
				SetWallTexture(379, Line.back, Side.top, "METAL2");
				break;
			}
			
			case '0D491365C1B88B7D1B603890100DD03E': // doom2.wad map18
			{
				// missing textures
				SetWallTexture(451, Line.front, Side.mid, "metal");
				SetWallTexture(459, Line.front, Side.mid, "metal");
				SetWallTexture(574, Line.front, Side.top, "grayvine");
				break;
			}
			
			case 'B5506B1E8F2FC272AD0C77B9E0DF5491': // doom2.wad map19
			{
				// missing textures
				SetWallTexture(355, Line.back, Side.top, "STONE2");
				SetWallTexture(736, Line.front, Side.top, "SLADWALL");
				SetWallTexture(1181, Line.back, Side.top, "MARBLE1");
				
				TextureID step4 = TexMan.CheckForTexture("STEP4", TexMan.Type_Wall);
				for(int i=0; i<3; i++)
				{
					SetWallTextureID(286+i, Line.back, Side.bottom, STEP4);
				}
				// Southwest teleporter in the teleporter room now works on
				// easier difficulties
				SetThingSkills(112, 31);
				break;
			}
			
			case 'EBDAC00E9D25D884B2C8F4B1F0390539': // doom2.wad map21
			{
				// push ceiling down in glitchy sectors above the stair switches
				OffsetSectorPlane(50, Sector.ceiling, -56);
				OffsetSectorPlane(54, Sector.ceiling, -56);
				break;
			}
			
			case '4AA9B3CE449FB614497756E96509F096': // Doom II MAP22
			{
				// Only use switch once to raise sector to rocket launcher
				SetLineFlags(120, 0, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '94893A0DC429A22ADC4B3A73DA537E16': // Doom II MAP25
			{
				// Missing texture at bloodfall near level start
				SetWallTexture(436, Line.back, Side.top, "STONE6");
				break;
			}
			
			case '1037366026AAB4B0CF11BAB27DB90E4E': // Doom II MAP26
			{
				// Missing texture at level exit
				SetWallTexture(761, Line.back, Side.top, "METAL2");
				break;
			}
			
			case '110F84DE041052B59307FAF0293E6BC0': // Doom II, map27
			{
				// Remove unreachable secret
				SetSectorSpecial(93, 0);
				// Missing texture
				SetWallTexture(582, Line.back, Side.top, "ZIMMER3");
				// Make line near switch to level exit passable
				SetLineFlags(342, 0, Line.ML_BLOCKING);
				// Can leave Pain Elemental room if stuck
				SetLineSpecial(580, Door_Open, 0, 16);
				SetLineActivation(580, SPAC_Use);
				SetLineSpecial(581, Door_Open, 0, 16);
				SetLineActivation(581, SPAC_Use);
				break;
			}
			
			case '84BB2C8ED2343C91136B87F1832E7CA5': // Doom II MAP28
			{
				// Missing textures
				TextureID ashwall6 = TexMan.CheckForTexture("ASHWALL6", TexMan.Type_Wall);
				for(int i=0; i<5; i++)
				{
					SetWallTextureID(103+i, Line.front, Side.top, ASHWALL6);
				}
				SetWallTexture(221, Line.front, Side.bottom, "BFALL1");
				SetWallTexture(391, Line.front, Side.top, "BIGDOOR5");
				SetWallTexture(531, Line.back, Side.top, "WOOD8");
				SetWallTexture(547, Line.back, Side.top, "WOOD8");
				SetWallTexture(548, Line.back, Side.top, "WOOD8");
				// Raise holes near level exit when walking over them
				for(int i=0; i<12; i++)
				{
					SetLineSpecial(584+i, Floor_RaiseToNearest, 5, 8);
					SetLineActivation(584+i, SPAC_Cross);
				}
				break;
			}
			
			case '20251EDA21B2F2ECF6FF5B8BBC00B26C': // Doom II, MAP29
			{
				// Missing textures on teleporters
				TextureID support3 = TexMan.CheckForTexture("SUPPORT3", TexMan.Type_Wall);
				for(int i=0;i<4;i++)
				{
					SetWallTextureID(405+i, Line.back, Side.bottom, SUPPORT3);
					SetWallTextureID(516+i, Line.back, Side.bottom, SUPPORT3);
					SetWallTextureID(524+i, Line.back, Side.bottom, SUPPORT3);
					SetWallTextureID(1146+i, Line.back, Side.bottom, SUPPORT3);
					SetWallTextureID(1138+i, Line.back, Side.bottom, SUPPORT3);
				}
				// Fix missing textures at switch with Arch-Vile.
				OffsetSectorPlane(152, Sector.ceiling, -32);
				SetWallTexture(603, Line.back, Side.top, "WOOD5");
				break;
			}
			
			case '915409A89746D6BFD92C7956BE6A0A2D': // Doom II: BFG Edition MAP33
			{
				// Missing textures on sector with a Super Shotgun at map start.
				TextureID rock2 = TexMan.CheckForTexture("ROCK2", TexMan.Type_Wall);
				for(int i=0; i<4; i++)
				{
					SetWallTextureID(567+i, Line.front, Side.bottom, ROCK2);
					SetWallTextureID(567+i, Line.back, Side.top, ROCK2);
				}
				// Tags the linedefs on the teleporter at the end of the level so that
				// it's possible to leave the room near the yellow keycard door.
				for(int i=0; i<2; i++)
				{
					SetLineSpecial(400+i, Teleport, 0, 36);
					SetLineSpecial(559+i, Teleport, 0, 36);
				}
				break;
			}

			case 'FF635FB9A2F076566299910F8C78F707': // nerve.wad, level04
			{
				SetSectorSpecial(868, 0);
				break;
			}

			case 'D94587625BA779644D58151A87897CF1': // heretic.wad e1m2
			{
				// Missing textures
				TextureID mossrck1 = TexMan.CheckForTexture("MOSSRCK1", TexMan.Type_Wall);
				SetWallTextureID( 477, Line.back,  Side.top, mossrck1);
				SetWallTextureID( 478, Line.back,  Side.top, mossrck1);
				SetWallTextureID( 479, Line.back,  Side.top, mossrck1);
				SetWallTextureID(1057, Line.front, Side.top, mossrck1);
				break;
			}
			
			case 'ADD0FAC41AFB0B3C9B9F3C0006F93805': // heretic.wad e1m3
			{
				// Broken door between the hallway that leads to a Torch 
				// and the passage that has a Bag of Holding at its end
				OffsetSectorPlane(86, Sector.floor,   -128);
				OffsetSectorPlane(86, Sector.ceiling, -128);
				break;
			}
			
			case '916318D8B06DAC2D83424B23E4B66531': // heretic.wad e1m4
			{
				// Wrong sector offsets
				OffsetSectorPlane( 0, Sector.ceiling, 8);
				OffsetSectorPlane( 1, Sector.ceiling, 8);
				OffsetSectorPlane( 2, Sector.ceiling, 8);
				OffsetSectorPlane( 3, Sector.ceiling, 8);
				OffsetSectorPlane( 4, Sector.ceiling, 8);
				OffsetSectorPlane( 6, Sector.ceiling, 8);
				OffsetSectorPlane( 6, Sector.floor,   8);
				OffsetSectorPlane(17, Sector.ceiling, 8);
				// Yellow key door
				OffsetSectorPlane(284, Sector.floor,   -8);
				OffsetSectorPlane(284, Sector.ceiling, -8);
				// Missing textures
				SetWallTexture(490, Line.back, Side.bottom, "GRSTNPB");
				TextureID woodwl = TexMan.CheckForTexture("WOODWL", TexMan.Type_Wall);
				SetWallTextureID( 722, Line.front, Side.bottom, woodwl);
				SetWallTextureID( 911, Line.front, Side.bottom, woodwl);
				SetWallTextureID(1296, Line.front, Side.bottom, woodwl);
				break;
			}
			
			case '397A0E17A39542E4E8294E156FAB0502': // heretic.wad e2m2
			{
				// Missing green door statues on easy and hard difficulties
				SetThingSkills(17, 31);
				SetThingSkills(18, 31);
				break;
			}
			
			case 'FAA0550BE9923B3A3332B4F7DB897A4A': // heretic.wad e2m7
			{
				// missing texture
				TextureID looserck = TexMan.CheckForTexture("LOOSERCK", TexMan.Type_Wall);
				SetWallTextureID( 629, Line.back,  Side.top, looserck);				
				break;
			}
			
			case 'CA3773ED313E8899311F3DD0CA195A68': // heretic.wad e3m6
			{
				// Quartz flask outside of map
				SetThingSkills(373, 0);
				// Missing wall torch on hard difficulty
				SetThingSkills(448, 31);
				// Missing textures
				TextureID mossrck1 = TexMan.CheckForTexture("MOSSRCK1", TexMan.Type_Wall);
				SetWallTextureID(343, Line.front, Side.top, mossrck1);
				SetWallTextureID(370, Line.front, Side.top, mossrck1);
				break;
			}
			
			case '5E3FCFDE78310BB89F92B1626A47D0AD': // heretic.wad E4M7
			{
				// Missing textures
				TextureID cstlrck = TexMan.CheckForTexture("CSTLRCK", TexMan.Type_Wall);
				SetWallTextureID(1274, Line.front, Side.top, cstlrck);
				SetWallTextureID(1277, Line.back,  Side.top, cstlrck);
				SetWallTextureID(1278, Line.front, Side.top, cstlrck);
				break;
			}

			case '30D1480A6D4F3A3153739D4CCF659C4E': // heretic.wad E4M8
			{
				// multiplayer teleporter prevents exit on cooperative
				SetThingFlags(78,MTF_DEATHMATCH);
				break;
			}

			case '6CDA2721AA1076F063557CF89D88E92B': // hexen.wad map08
			{
				// Amulet of warding accidentally shifted outside of map
				SetThingXY(256,-1632,2352);
				// Icon of the defender outside of map
				SetThingSkills(261,0);
				break;
			}

			case '39C594CAC07EE51C80F757DA465FCC94': // strife1.wad map10
			{
				// fix the shooting range by matching sector 138 and 145 properties together
				OffsetSectorPlane(145, Sector.floor,  -32);
				OffsetSectorPlane(145, Sector.ceiling, 40);
				SetSectorTexture(145, Sector.floor, "F_CONCRP");
				SetSectorLight(138, 192);
				SetWallTexture(3431, Line.back, Side.top, "BRKGRY01");
				break;
			}

			case '8D7A24B169717907DDA8399D8C1655DF': // strife1.wad map15
			{
				SetWallTexture(319, Line.back, Side.top, "WALTEK21");
				break;
			}

			case 'D5FD90FA7A8133E7BFED682D3D313962': // strife1.wad map21
			{
				SetWallTexture(603, Line.front, Side.bottom, "IRON04");
				break;
			}

			case '775CBC35C0A58326FE87AAD638FF9E2A': // Strife1.wad map29
			case 'A75099ACB622C7013EE737480FCB0D67': // SVE.wad map29
				// disable teleporter that would always teleport into a blocking position.
				ClearLineSpecial(271);
				break;

			case 'DB31D71B11E3E4393B9C0CCB44A8639F': // rop_2015.wad e1m5
			{
				// Lower floor a bit so secret switch becomes accessible
				OffsetSectorPlane(527, Sector.floor, -32);
				break;
			}

			case 'CC3911090452D7C39EC8B3D97CEFDD6F': // jenesis.wad map16
			{
				// Missing texture with hardware renderer because of wrongly lowered sector
				ClearSectorTags(483);
				break;
			}

			case 'B68EB7CFB4CC481796E2919B9C16DFBD':  // Moc11.wad e1m6
			{
				SetVertex(1650, -3072, 2671);
				SetVertex(1642, -2944, 2671);
				break;
			}

			case '55BF5BFAF086C904E7258258F9700155': // Eternal Doom map02
			{
				// unreachable monsters
				SetThingFlags(274, GetThingFlags(274) | MTF_NOCOUNT);
				SetThingFlags(275, GetThingFlags(275) | MTF_NOCOUNT);
				SetThingFlags(276, GetThingFlags(276) | MTF_NOCOUNT);
				SetThingFlags(277, GetThingFlags(277) | MTF_NOCOUNT);
				SetThingFlags(278, GetThingFlags(278) | MTF_NOCOUNT);
				SetThingFlags(279, GetThingFlags(279) | MTF_NOCOUNT);
				SetThingFlags(280, GetThingFlags(280) | MTF_NOCOUNT);
				SetThingFlags(281, GetThingFlags(281) | MTF_NOCOUNT);
				break;
			}

			case '5C594C67CF7721005DE71429F9811370': // Eternal Doom map03
			{
				// fix broken staircase. The compatibility option is not sufficient
				// to reliably handle this so clear the tags from the unwanted sectors.
				ClearSectorTags(212);
				ClearSectorTags(213);
				ClearSectorTags(214);
				// unreachable secret
				SetSectorSpecial(551, 0);
				break;
			}

			case '9A4615498C3451413F1CD3D15099ACC7': // Eternal Doom map05
			{
				// an imp and two cyberdemons are located at the softlock area
				SetThingFlags(272, GetThingFlags (272) | MTF_NOCOUNT);
				SetThingFlags(273, GetThingFlags (273) | MTF_NOCOUNT);
				SetThingFlags(274, GetThingFlags (274) | MTF_NOCOUNT);
				break;
			}

			case '8B55842D5A509902738040AF10B4E787': // Eternal Doom map10
			{
				// soulsphere at the end of the level is there merely to replicate the start of the next map
				SetThingFlags(548, GetThingFlags (548) | MTF_NOCOUNT);
				break;
			}

			case 'E5B4379151C2010B966CA37A9818C901': // Eternal Doom map12
			{
				// unreachable baron
				SetThingFlags(177, GetThingFlags(177) | MTF_NOCOUNT);
				break;
			}
		
			case 'DCE862393CAAA6FF1294FB7056B53057': // UAC Ultra map07
			{
				// Contains a scroller depending on Boom side effects
				SetLineSpecial(391, Sector_CopyScroller, 99, 6);
				break;
			}

			case '9D50EBE17CEC78938C7A668DB0768611': // Strain map07
			{
				// Make the exit accessible
				SetLineFlags(1021, 0, Line.ML_BLOCKING);
				break;
			}

			case '3D8ED20BF5CAAE6D6AE0E10999C75084': // hgarden.pk3 map01
			{
				// spawn trees on top of arches
				SetThingZ(399, 168);
				SetThingZ(400, 168);
				SetThingZ(401, 168);
				SetThingZ(402, 168);
				SetThingZ(403, 168);
				SetThingZ(404, 168);
				break;
			}

			case '6DC9F6CCEAE7A91AEC48EBE506F22BC4': // void.wad MAP01
			{
				// Slightly squash the pillars in the starting room with "stimpacks"
				// floating on them so that they can be obtained.
				OffsetSectorPlane( 62, Sector.floor, -8);
				OffsetSectorPlane( 63, Sector.floor, -8);
				OffsetSectorPlane(118, Sector.floor, -8);
				OffsetSectorPlane(119, Sector.floor, -8);
				for (int i = 0; i < 8; ++i)
				{
					SetWallYScale(286 + i, Line.front, Side.bottom, 1.090909);
					SetWallYScale(710 + i, Line.front, Side.bottom, 1.090909);
				}
				break;
			}
			
			case '57386AEF275684BA06756359B08F4391': // Perdition's Gate MAP03
			{
				// Stairs where one sector is too thin to score.
				SetSectorSpecial(227, 0);
				break;
			}
			
			case 'F1A9938C4FC3906A582AB7D5088B5F87': // Perdition's Gate MAP12
			{
				// Sector unintentionally left as a secret near switch
				SetSectorSpecial(112, 0);
				break;
			}
			
			case '5C419E581D9570F44A24163A83032086': // Perdition's Gate MAP27
			{
				// Sectors unintentionally left as secrets and cannot be scored
				SetSectorSpecial(338, 0);
				SetSectorSpecial(459, 0);
				break;
			}

			case 'FCCA97FC851F6473EAA069F74247B317': // pg-raw.wad map31
			{
				SetLineSectorRef(331, Line.front, 74);
				SetLineSectorRef(326, Line.front, 74);
				SetLineSectorRef(497, Line.front, 74);
				SetLineSectorRef(474, Line.front, 74);
				SetLineSectorRef(471, Line.front, 74);
				SetLineSectorRef(327, Line.front, 74);
				SetLineSectorRef(328, Line.front, 74);
				SetLineSectorRef(329, Line.front, 74);
				AddSectorTag(74, 4);
				SetLineSpecial(357, Transfer_Heights, 4, 6);
				break;
			}
			
			case '5379C080299EB961792B50AD96821543': // Hell to Pay MAP14
			{
				// Two secrets are unreachable without jumping and crouching.
				SetSectorSpecial(82, 0);
				SetSectorSpecial(83, 0);
				break;
			}
			
			case '7837B5334A277F107515D649BCEFB682': // Hell to Pay MAP22
			{
				// Four enemies (six if multiplayer) never spawn in the map,
				// so the lines closest to them should teleport them instead.
				SetLineSpecial(1835, Teleport, 0, 40);
				SetLineActivation(1835, SPAC_MCross);
				SetLineFlags(1835, Line.ML_REPEAT_SPECIAL);
				
				SetLineSpecial(1847, Teleport, 0, 40);
				SetLineActivation(1847, SPAC_MCross);
				SetLineFlags(1847, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '1A1AB6415851B9F17715A0C36412752E': // Hell to Pay MAP24
			{
				// Remove Chaingunner far below the map, making 100% kills
				// impractical.
				SetThingFlags(70, 0);
				break;
			}
			
			case 'A7ACB57A2CAF17434D0DFE0FAC0E0480': // Hell to Pay MAP28
			{
				// Three Lost Souls placed outside the map for some reason.
				for(int i=0; i<3; i++)
				{
					SetThingFlags(217+i, 0);
				}
				break;
			}
			
			case '2F1A18633C30E938B50B6D928C730CB6': // Hell to Pay MAP29
			{
				// Three Lost Souls placed outside the map, again...
				for(int i=0; i<3; i++)
				{
					SetThingFlags(239+i, 0);
				}
				break;
			}
			
			case '712BB4CFBD0753178CA0C6814BE4C288': // beta version of map12 BTSX_E1
			{
				// patch some rendering glitches that are problematic to detect
				AddSectorTag(545, 32000);
				AddSectorTag(1618, 32000);
				SetLineSpecial(2853, Sector_Set3DFloor, 32000, 4);
				AddSectorTag(439, 32001);
				AddSectorTag(458, 32001);
				SetLineSpecial(2182, Sector_Set3DFloor, 32001, 4);
				AddSectorTag(454, 32002);
				AddSectorTag(910, 32002);
				SetLineSpecial(2410, Sector_Set3DFloor, 32002, 4, 1);
				break;
			}
			
			case '5A24FC83A3F9A2D6D54AF04E2E96684F': // AV.WAD MAP01
			{
				SetLineSectorRef(225, Line.back, 36);
				SetLineSectorRef(222, Line.back, 36);
				SetLineSectorRef(231, Line.back, 36);

				SetLineSectorRef(223, Line.back, 36);
				SetLineSectorRef(224, Line.back, 36);
				SetLineSectorRef(227, Line.back, 36);

				SetLineSectorRef(229, Line.back, 39);
				SetLineSectorRef(233, Line.back, 39);

				TextureID nukage = TexMan.CheckForTexture("NUKAGE1", TexMan.Type_Flat);
				SetWallTextureID(222, Line.front, Side.bottom, nukage);
				SetWallTextureID(223, Line.front, Side.bottom, nukage);
				SetWallTextureID(224, Line.front, Side.bottom, nukage);
				SetWallTextureID(225, Line.front, Side.bottom, nukage);
				SetWallTextureID(227, Line.front, Side.bottom, nukage);
				SetWallTextureID(231, Line.front, Side.bottom, nukage);
				SetWallTextureID(229, Line.front, Side.bottom, nukage);
				SetWallTextureID(233, Line.front, Side.bottom, nukage);

				for(int i = 0; i < 8; i++)
				{
					SetLineSectorRef(i+234, Line.back, 37);
					SetLineSectorRef(i+243, Line.back, 37);
					SetWallTextureID(i+234, Line.back, Side.bottom, nukage);
					SetWallTextureID(i+243, Line.back, Side.bottom, nukage);
				}

				SetLineSpecial(336, Transfer_Heights, 32000, 6);
				AddSectorTag(40, 32000);
				AddSectorTag(38, 32000);
				AddSectorTag(37, 32000);
				AddSectorTag(34, 32000);
				break;
			}
			
			case '32FADD80710CAFCC2B09B4610C3340B3': // ksutra.wad map01
			{
				// This rebuilds the ending pit with a 3D floor.
				for(int i = Line.front; i <= Line.back; i++)
				{
					SetLineSectorRef(509, i, 129);
					SetLineSectorRef(510, i, 129);
					SetLineSectorRef(522, i, 129);
					SetLineSectorRef(526, i, 129);
					SetLineSectorRef(527, i, 129);
					for(int j = 538; j <= 544; j++)
					{
						SetLineSectorRef(j, i, 129);
					}						
					for(int j = 547; j <= 552; j++)
					{
						SetLineSectorRef(j, i, 129);
					}					
				}
				AddSectorTag(129, 32000);
				SetSectorLight(129, 160);
				for(int i = 148; i <= 151; i++)
				{
					AddSectorTag(i, 32000);
					SetSectorLight(i, 160);
				}
				SetSectorTexture(126, Sector.Ceiling, "GRASS1");
				OffsetSectorPlane(126, Sector.Floor, 72);
				OffsetSectorPlane(126, Sector.Ceiling, 128);
				SetLineSpecial(524, Sector_Set3DFloor, 32000, 1, 0, 255);
				SetWallTexture(524, Line.Front, Side.Mid, "ASHWALL3");
				SetWallTexture(537, Line.Front, Side.Bottom, "ASHWALL3");
				SetWallTexture(536, Line.Front, Side.Bottom, "ASHWALL3");
				SetWallTexture(546, Line.Front, Side.Bottom, "ASHWALL3");
				SetWallTexture(449, Line.Front, Side.Mid, "-");
				break;
			}

			case '9C14350A111C3DC6A8AF04D950E6DDDB': // ma_val.pk3 map01
			{
				// Missing wall textures with hardware renderer
				OffsetSectorPlane(7072, Sector.floor, -48);
				OffsetSectorPlane(7073, Sector.floor, -32);
				// Missing teleporting monsters
				SetThingFlags(376, 0x200);
				SetThingFlags(377, 0x300);
				for (int i = 437; i < 449; ++i)
				{
					SetThingFlags(i, 0x600);
				}
				// Stuck imp
				SetThingXY(8, 1200, -1072);
				break;
			}

			case '5B8689912D21E91D899C61BBBDD44D7C': // altar of evil.wad map01
			{
				// Missing teleport destination on easy skill
				SetThingSkills(115, 31);
				break;
			}

			case 'CCF699953746087E46185B2A40D9F8AF': // satanx.wad map01
			{
				// Restore monster cross flag for DeHackEd friendly marine
				GetDefaultActor('WolfensteinSS').bActivateMCross = true;
				break;
			}
			
			case 'D67CECE3F60083383DF992B8C824E4AC': // Icarus: Alien Vanguard MAP13
			{
				// Moves sector special to platform with Berserk powerup. The
				// map's only secret can now be scored.
				SetSectorSpecial(119, 0);
				SetSectorSpecial(122, 1024);
				break;
			}
			
			case '61373587339A768854E2912CC99A4781': // Icarus: Alien Vanguard MAP15
			{
				// Can press use on the lift to reveal the secret Shotgun,
				// making 100% secrets possible.
				SetLineSpecial(222, Plat_DownWaitUpStayLip, 11, 64, 105, 0);
				SetLineActivation(222, SPAC_Use);
				SetLineFlags(222, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '9F66B0797925A09D4DC0725540F8EEF7': // Icarus: Alien Vanguard MAP16
			{
				// Can press use on the walls at the secret Rocket Launcher in
				// case of getting stuck.
				for(int i=0; i<7; i++)
				{
					SetLineSpecial(703+i, Plat_DownWaitUpStayLip, 14, 64, 105, 0);
					SetLineActivation(703+i, SPAC_Use);
					SetLineFlags(703+i, Line.ML_REPEAT_SPECIAL);
				}
				break;
			}
			
			case '09645D198010BF634EF0DE3EFCB0052C': // Flashback to Hell MAP12
			{
				// Can press use behind bookshelf in case of getting stuck.
				SetLineSpecial(4884, Plat_DownWaitUpStayLip, 15, 32, 105, 0);
				SetLineActivation(4884, SPAC_UseBack);
				SetLineFlags(4884, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '7FB847B522DE80D0B2A217E1EF8D1A15': // av.wad map28
			{
				// Fix the soulsphere in a secret area (sector 324)
				// so that it doesn't end up in an unreachable position.
				SetThingXY(516, -934, 48);
				break;
			}
			
			case '11EA5B8357DEB70A8F00900117831191': // kdizd_12.pk3 z1m3
			{
				// Fix incorrectly tagged underwater sector which causes render glitches.
				AddSectorTag(7857, 82);
				break;
			}
			
			case '7B1EB6C1231CD03E90F4A1C0D51A8B6D': // ur_final.wad map17
			{
				SetLineSpecial(3020, Transfer_Heights, 19);
				break;
			}
			
			case '01592ACF001C534076556D9E1B5D85E7': // Darken2.wad map12
			{
				// fix some holes the player can fall in. This map went a bit too far with lighting hacks depending on holes in the floor.
				OffsetSectorPlane(126, Sector.floor, 1088);
				level.sectors[126].SetPlaneLight(Sector.floor, level.sectors[125].GetLightLevel() - level.sectors[126].GetLightLevel());
				OffsetSectorPlane(148, Sector.floor, 1136);
				level.sectors[148].SetPlaneLight(Sector.floor, level.sectors[122].GetLightLevel() - level.sectors[148].GetLightLevel());
				OffsetSectorPlane(149, Sector.floor, 1136);
				level.sectors[149].SetPlaneLight(Sector.floor, level.sectors[122].GetLightLevel() - level.sectors[149].GetLightLevel());
				OffsetSectorPlane(265, Sector.floor, 992);
				level.sectors[265].SetPlaneLight(Sector.floor, level.sectors[264].GetLightLevel() - level.sectors[265].GetLightLevel());
				OffsetSectorPlane(279, Sector.floor, 1072);
				level.sectors[279].SetPlaneLight(Sector.floor, level.sectors[267].GetLightLevel() - level.sectors[279].GetLightLevel());
				SetSectorTexture(279, Sector.floor, "OMETL13");
				OffsetSectorPlane(280, Sector.floor, 1072);
				level.sectors[280].SetPlaneLight(Sector.floor, level.sectors[267].GetLightLevel() - level.sectors[280].GetLightLevel());
				SetSectorTexture(280, Sector.floor, "OMETL13");
				OffsetSectorPlane(281, Sector.floor, 1072);
				level.sectors[281].SetPlaneLight(Sector.floor, level.sectors[267].GetLightLevel() - level.sectors[281].GetLightLevel());
				SetSectorTexture(281, Sector.floor, "OMETL13");
				OffsetSectorPlane(292, Sector.floor, 1056);
				level.sectors[292].SetPlaneLight(Sector.floor, level.sectors[291].GetLightLevel() - level.sectors[292].GetLightLevel());
				OffsetSectorPlane(472, Sector.floor, 1136);
				level.sectors[472].SetPlaneLight(Sector.floor, level.sectors[216].GetLightLevel() - level.sectors[472].GetLightLevel());
				OffsetSectorPlane(473, Sector.floor, 1136);
				level.sectors[473].SetPlaneLight(Sector.floor, level.sectors[216].GetLightLevel() - level.sectors[473].GetLightLevel());
				OffsetSectorPlane(526, Sector.floor, 1024);
				level.sectors[526].SetPlaneLight(Sector.floor, level.sectors[525].GetLightLevel() - level.sectors[526].GetLightLevel());
				OffsetSectorPlane(527, Sector.floor, 1024);
				level.sectors[527].SetPlaneLight(Sector.floor, level.sectors[500].GetLightLevel() - level.sectors[527].GetLightLevel());
				OffsetSectorPlane(528, Sector.floor, 1024);
				level.sectors[528].SetPlaneLight(Sector.floor, level.sectors[525].GetLightLevel() - level.sectors[528].GetLightLevel());
				OffsetSectorPlane(554, Sector.floor, 1024);
				level.sectors[554].SetPlaneLight(Sector.floor, level.sectors[500].GetLightLevel() - level.sectors[554].GetLightLevel());
				OffsetSectorPlane(588, Sector.floor, 928);
				level.sectors[588].SetPlaneLight(Sector.floor, level.sectors[587].GetLightLevel() - level.sectors[588].GetLightLevel());
				OffsetSectorPlane(604, Sector.floor, 1056);
				level.sectors[604].SetPlaneLight(Sector.floor, level.sectors[298].GetLightLevel() - level.sectors[604].GetLightLevel());
				OffsetSectorPlane(697, Sector.floor, 1136);
				level.sectors[697].SetPlaneLight(Sector.floor, level.sectors[696].GetLightLevel() - level.sectors[697].GetLightLevel());
				OffsetSectorPlane(698, Sector.floor, 1136);
				level.sectors[698].SetPlaneLight(Sector.floor, level.sectors[696].GetLightLevel() - level.sectors[698].GetLightLevel());
				OffsetSectorPlane(699, Sector.floor, 1136);
				level.sectors[699].SetPlaneLight(Sector.floor, level.sectors[696].GetLightLevel() - level.sectors[699].GetLightLevel());
				OffsetSectorPlane(700, Sector.floor, 1136);
				level.sectors[700].SetPlaneLight(Sector.floor, level.sectors[696].GetLightLevel() - level.sectors[700].GetLightLevel());
				break;
			}
			
			case '3B1F637295F5669E99BE63F1B1CA29DF': // titan426.wad map01
			{
				// Missing teleport destinations on easy skill
				SetThingSkills(138, 31); // secret
				SetThingSkills(1127, 31); // return from exit room
				break;
			}

			case '5E9AF879343D6E44E429F91D57777D26': // cchest.wad map16
			{
				// Fix misplaced vertex
				SetVertex(202, -2, -873);
				break;
			}
			case 'E9EB4D16CA7E491E98D61615E4613E70': // sigil.wad e5m2
			{
				// Floating Skulls missing in lower difficulties
				SetThingSkills(113, 31);
				SetThingSkills(114, 31);
				break;
			}
			case 'C43B99F34E5211F9AF24459842852B0D': // sigil.wad e5m4
			{
				// Fix missing texture on the Baron-invulnerability-secret platform
				SetWallTexture(1926, Line.back, Side.bottom, "MARBLE3");
				break;
			}
			case 'A7C4FC8CAEB3E375B7214E35C6298B03': // Illusions of Home e1m6
			{
				// Convert zero-tagged GR door into regular open-stay door to fix map
				SetLineActivation(37, SPAC_Use);
				SetLineSpecial(37, Door_Open, 0, 16);
				SetLineActivation(203, SPAC_Use);
				SetLineSpecial(203, Door_Open, 0, 16);
				break;
			}
			case '5084755C29FB0A1912113E36F37C958A': // Illusions of Home e3m4
			{
				// Fix action of final switch
				SetLineSpecial(765, Door_Open, 6, 16);
				break;
			}
			case '0EF86635676FD512CE0E962040125553': // Illusions of Home e3m7
			{
				// Fix red key and missing texture in red key area
				SetThingFlags(247, 2016);
				SetThingSkills(247, 31);
				SetWallTexture(608, Line.back, Side.bottom, "GRAY5");
				break;
			}

			case '63BDD083A98A48C04B8CD58AA857F77D': // Scythe MAP22
			{
				// Wall behind start creates HOM in software renderer due to weird sector
				OffsetSectorPlane(236, Sector.Floor, -40);
				break;
			}

			case '1C795660D2BA9FC93DA584C593FD1DA3': // Scythe 2 MAP17
			{
				// Texture displays incorrectly in hardware renderer
				SetVertex(2687, -4540, -1083); //Fixes bug with minimal effect on geometry
				break;
			}
			case '7483D7BDB8375358F12D146E1D63A85C': // Scythe 2 MAP24
			{
				// Missing texture
				TextureID adel_q62 = TexMan.CheckForTexture("ADEL_Q62", TexMan.Type_Wall);
				SetWallTextureID(7775, Line.front, Side.bottom, adel_q62);
				break;
			}

			case '16E621E46F87418F6F8DB71D68433AE0': // Hell Revealed MAP23
			{
				// Arachnotrons near beginning sometimes don't spawn if imps block
				// their one-time teleport. Make these teleports repeatable to ensure
				// maxkills are always possible.
				SetLineFlags(2036, Line.ML_REPEAT_SPECIAL);
				SetLineFlags(2038, Line.ML_REPEAT_SPECIAL);
				SetLineFlags(2039, Line.ML_REPEAT_SPECIAL);
				SetLineFlags(2040, Line.ML_REPEAT_SPECIAL);
				break;
			}
			case '145C4DFCF843F2B92C73036BA0E1D98A': // Hell Revealed MAP26
			{
				// The 4 archviles that produce the ghost monsters cannot be killed
				// Make them not count so they still produce ghosts while allowing 100% kills.
				SetThingFlags(320, GetThingFlags (320) | MTF_NOCOUNT);
				SetThingFlags(347, GetThingFlags (347) | MTF_NOCOUNT);
				SetThingFlags(495, GetThingFlags (495) | MTF_NOCOUNT);
				SetThingFlags(496, GetThingFlags (496) | MTF_NOCOUNT);
				break;
			}

			case '0E379EEBEB189F294ED122BC60D10A68': // Hellbound MAP29
			{
				// Remove the cyberdemons stuck in the closet boxes that cannot teleport.
				SetThingFlags(2970,0);
				SetThingFlags(2969,0);
				SetThingFlags(2968,0);
				SetThingFlags(2169,0);
				SetThingFlags(2168,0);
				SetThingFlags(2167,0);
				break;
			}

			case '66B931B03618EDE5C022A1EC87189158': // Restoring Deimos MAP03
			{
				// Missing teleport destination on easy skill
				SetThingSkills(62, 31);
				break;
			}

			case '6B9D31106CE290205A724FA61C165A80': // Restoring Deimos MAP07
			{
				// Missing map spots on easy skill
				SetThingSkills(507, 31);
				SetThingSkills(509, 31);
				break;
			}

			case '17314071AB76F4789763428FA2E8DA4C': // Skulldash Expanded Edition MAP04
			{
				// Missing teleport destination on easy skill
				SetThingSkills(164, 31);
				break;
			}
			
			case '1B27E04F707E7988800582F387F609CD': // One against hell (ONE1.wad)
			{
				//Turn map spots into teleport destinations so that teleports work
				SetThingEdNum(19, 14);
				SetThingEdNum(20, 14);
				SetThingEdNum(22, 14);
				SetThingEdNum(23, 14);
				SetThingEdNum(34, 14);
				SetThingEdNum(35, 14);
				SetThingEdNum(36, 14);
				SetThingEdNum(39, 14);
				SetThingEdNum(40, 14);
				SetThingEdNum(172, 14);
				SetThingEdNum(173, 14);
				SetThingEdNum(225, 14);
				SetThingEdNum(226, 14);
				SetThingEdNum(247, 14);
				break;
			}
			
			case '25E178C981BAC28BA586B3B0A2A0FD72': // swan fox doom v2.4.wad map13
			{
				//Actors with no game mode will now appear
				SetThingFlags(0, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				for(int i = 2; i < 452; i++)
					SetThingFlags(i, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				//Awaken monsters with Thing_Hate, since NoiseAlert is unreliable
				SetThingID(465, 9);
				SetLineSpecial(1648, 177, 9);
				for(int i = 1650; i <= 1653; i++)
					SetLineSpecial(i, 177, 9);
				SetLineSpecial(1655, 177, 9);
				break;
			}
		
			case 'FDFB3D209CC0F3706AAF3E51646003D5': // swan fox doom v2.4.wad map23
			{
				//Missing Teleport Destination on Easy/Normal difficulty
				SetThingSkills(650, SKILLS_ALL);
				//Fix the exit portal to allow walking into it instead of shooting it
				SetLineSpecial(1970, Exit_Normal, 0);
				SetLineActivation(1970, SPAC_Cross);
				ClearLineSpecial(2010);
				SetWallTexture(1966, Line.back, Side.mid, "TELR1");
				OffsetSectorPlane(170, Sector.floor, -120);
				break;
			}

			case 'BC969ED0191CCD9B69B316864362B0D7': // swan fox doom v2.4.wad map24
			{
				//Actors with no game mode will now appear
				for(int i = 1; i < 88; i++)
					SetThingFlags(i, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				break;
			}

			case 'ED740248422026326650F6A4BC1C0A5A': // swan fox doom v2.4.wad map25
			{
				//Actors with no game mode will now appear
				for(int i = 0; i < 8; i++)
					SetThingFlags(i, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				for(int i = 9; i < 26; i++)
					SetThingFlags(i, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				break;
			}
			
			case '52F532F95E2D5862E56F7214FA5C5C59': // Toon Doom II (toon2b.wad) map10
			{
				//This lift needs to be repeatable to prevent trapping the player
				SetLineSpecial(224, Plat_DownWaitUpStayLip, 5, 32, 105);
				SetLineActivation(224, SPAC_Use);
				SetLineFlags(224, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '3345D12AD97F20EDCA5E27BA4288F758': // Toon Doom II (toon2b.wad) map30
			{
				//These doors need to be repeatable to prevent trapping the player
				SetLineSpecial(417, Door_Raise, 17, 16, 150);
				SetLineActivation(417, SPAC_Use);
				SetLineFlags(417, Line.ML_REPEAT_SPECIAL);
				SetLineSpecial(425, Door_Raise, 15, 16, 150);
				SetLineActivation(425, SPAC_Use);
				SetLineFlags(425, Line.ML_REPEAT_SPECIAL);
				SetLineSpecial(430, Door_Raise, 16, 16, 150);
				SetLineActivation(430, SPAC_Use);
				SetLineFlags(430, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case 'BA1288DF7A7AD637948825EA87E18728': // Valletta's Doom Nightmare (vltnight.wad) map02
			{
				//This door's backside had a backwards linedef so it wouldn't work
				FlipLineCompletely(306);
				//Set the exit to point to Position 0, since that's the only one on the next map
				SetLineSpecial(564, Exit_Normal, 0);
				break;
			}
			
			case '9B966DA88265AC8972B7E15C86928AFB': // Clavicula Nox: Revised Edition map01
			{
				// All swimmable water in Clavicula Nox is Vavoom-style, and doesn't work.
				// Change it to ZDoom-style and extend the control sector heights to fill.
				// Lava also sometimes needs the damage effect added manually.
				SetLineSpecial(698, 160, 4, 2, 0, 128);
				OffsetSectorPlane(121, Sector.floor, -32);
				break;
			}
			
			case '9FD0C47C2E132F64B48CA5BFBDE47F1D': // clavnoxr map02
			{
				SetLineSpecial(953, 160, 12, 2, 0, 128);
				OffsetSectorPlane(139, Sector.floor, -24);
				SetLineSpecial(989, 160, 13, 2, 0, 128);
				OffsetSectorPlane(143, Sector.floor, -24);
				SetLineSpecial(1601, 160, 19, 2, 0, 128);
				SetLineSpecial(1640, 160, 20, 2, 0, 128);
				SetLineSpecial(1641, 160, 21, 2, 0, 128);
				OffsetSectorPlane(236, Sector.floor, -96);
				break;
			}
			
			case '969B9691007490CF022B632B2729CA49': // clavnoxr map03
			{
				SetLineSpecial(96, 160, 1, 2, 0, 128);
				OffsetSectorPlane(11, Sector.floor, -80);
				SetLineSpecial(955, 160, 13, 2, 0, 128);
				OffsetSectorPlane(173, Sector.floor, -16);
				SetLineSpecial(1082, 160, 14, 2, 0, 128);
				OffsetSectorPlane(195, Sector.floor, -16);
				SetLineSpecial(1111, 160, 15, 2, 0, 128);
				OffsetSectorPlane(203, Sector.floor, -16);
				SetLineSpecial(1115, 160, 16, 2, 0, 128);
				OffsetSectorPlane(204, Sector.floor, -16);
				SetLineSpecial(1163, 160, 17, 2, 0, 128);
				OffsetSectorPlane(216, Sector.floor, -56);
				SetLineSpecial(1169, 160, 18, 2, 0, 128);
				OffsetSectorPlane(217, Sector.floor, -16);
				break;
			}
			
			case '748EDAEB3990F13B22C13C593631B2E6': // clavnoxr map04
			{
				SetLineSpecial(859, 160, 22, 2, 0, 128);
				SetLineSpecial(861, 160, 23, 2, 0, 128);
				OffsetSectorPlane(172, Sector.floor, -168);
				break;
			}
			
			case 'A066B0B432FAE8ED20B83383F9E03E12': // clavnoxr map05
			{
				SetLineSpecial(1020, 160, 2, 2, 0, 128);
				SetSectorSpecial(190, 69);
				OffsetSectorPlane(190, Sector.floor, -24);
				break;
			}
			
			case '5F36D758ED26CAE907FA79902330877D': // clavnoxr map06
			{
				SetLineSpecial(1086, 160, 1, 2, 0, 128);
				SetLineSpecial(60, 160, 4, 2, 0, 128);
				SetLineSpecial(561, 160, 2, 2, 0, 128);
				SetLineSpecial(280, 160, 3, 2, 0, 128);
				SetLineSpecial(692, 160, 5, 2, 0, 128);
				SetLineSpecial(1090, 160, 7, 2, 0, 128);
				SetLineSpecial(1091, 160, 8, 2, 0, 128);
				SetLineSpecial(1094, 160, 9, 2, 0, 128);
				SetLineSpecial(1139, 160, 12, 2, 0, 128);
				SetLineSpecial(1595, 160, 16, 2, 0, 128);
				SetLineSpecial(1681, 160, 17, 2, 0, 128);
				SetLineSpecial(1878, 160, 20, 2, 0, 128);
				SetLineSpecial(1882, 160, 22, 2, 0, 128);
				OffsetSectorPlane(8, Sector.floor, -64);
				OffsetSectorPlane(34, Sector.floor, -24);
				OffsetSectorPlane(64, Sector.floor, -16);
				OffsetSectorPlane(102, Sector.floor, -16);
				OffsetSectorPlane(170, Sector.floor, -64);
				OffsetSectorPlane(171, Sector.floor, -64);
				OffsetSectorPlane(178, Sector.floor, -16);
				OffsetSectorPlane(236, Sector.floor, -16);
				OffsetSectorPlane(250, Sector.floor, -16);
				OffsetSectorPlane(283, Sector.floor, -64);
				OffsetSectorPlane(284, Sector.floor, -56);
				break;
			}
			
			case 'B7E98C1EA1B38B707ADA8097C25CFA75': // clavnoxr map07
			{
				SetLineSpecial(1307, 160, 22, 2, 0, 128);
				SetLineSpecial(1305, 160, 23, 2, 0, 128);
				OffsetSectorPlane(218, Sector.floor, -64);
				break;
			}
			
			case '7DAB2E8BB5759D742211505A3E5054D1': // clavnoxr map08
			{
				SetLineSpecial(185, 160, 1, 2, 0, 128);
				SetLineSpecial(735, 160, 13, 2, 0, 128);
				OffsetSectorPlane(20, Sector.floor, -16);
				SetSectorSpecial(20, 69);
				OffsetSectorPlane(102, Sector.floor, -64);
				break;
			}
			
			case 'C17A9D1350399E251C70711EB22856AE': // clavnoxr map09
			{
				SetLineSpecial(63, 160, 1, 2, 0, 128);
				SetLineSpecial(84, 160, 2, 2, 0, 128);
				SetLineSpecial(208, 160, 4, 2, 0, 128);
				SetLineSpecial(326, 160, 6, 2, 0, 128);
				SetLineSpecial(433, 160, 10, 2, 0, 128);
				OffsetSectorPlane(6, Sector.floor, -64);
				OffsetSectorPlane(9, Sector.floor, -64);
				OffsetSectorPlane(34, Sector.floor, -64);
				OffsetSectorPlane(59, Sector.floor, -64);
				SetSectorSpecial(75, 69);
				OffsetSectorPlane(75, Sector.floor, -64);
				break;
			}
			
			case 'B6BB8A1792FE51C773E6770CD91DB618': // clavnoxr map11
			{
				SetLineSpecial(1235, 160, 23, 2, 0, 128);
				SetLineSpecial(1238, 160, 24, 2, 0, 128);
				OffsetSectorPlane(240, Sector.floor, -80);
				break;
			}
			
			case 'ADDF57B80E389F86D324571D43F3CAB7': // clavnoxr map12
			{
				SetLineSpecial(1619, 160, 19, 2, 0, 128);
				SetLineSpecial(1658, 160, 20, 2, 0, 128);
				SetLineSpecial(1659, 160, 21, 2, 0, 128);
				OffsetSectorPlane(254, Sector.floor, -160);				
				// Raising platforms in MAP12 didn't work, so this will redo them.
				SetLineSpecial(1469, 160, 6, 1, 0, 255);
				OffsetSectorPlane(65, Sector.floor, -8);
				OffsetSectorPlane(65, Sector.ceiling, 8);
				break;
			}

			case '75E2685CA8AB29108DBF1AC98C5450AC': // Ancient Beliefs (beliefs.wad) map01
			{
				//Actors with no game mode will now appear
				SetThingFlags(0, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				for(int i = 2; i < 7; i++)
					SetThingFlags(i, MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH);
				break;
			}

			case 'C4850382A78BF637AC9FC58153E03C87': // sapphire.wad map01
			{
				SetLineSpecial(11518, 9, 0, 0, 0, 0);
				break;
			}
			
			case 'BA530202AF0BA0C6CBAE6A0C7076FB72': // Requiem MAP04
			{
				// Flag deathmatch berserk for 100% items in single-player
				SetThingFlags(284, 17);
				// Make Hell Knight trap switch repeatable to prevent softlock
				SetLineFlags(823, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '104415DDEBCAFB9783CA60807C46B57D': // Requiem MAP05
			{
				// Raise spectre pit near soulsphere if player drops into it
				for(int i = 0; i < 4; i++)
				{
					SetLineSpecial(2130+i, Floor_LowerToHighest, 28, 8, 128);
					SetLineActivation(2130+i, SPAC_Cross);
				}
				break;
			}
			
			case '1B0AF5286D4E914C5E903BC505E6A844': // Requiem MAP06
			{
				// Flag deathmatch berserks for 100% items in single-player
				SetThingFlags(103, 17);
				SetThingFlags(109, 17);
				// Shooting the cross before all Imps spawn in can make 100%
				// kills impossible, add line actions and change sector tag
				for(int i = 0; i < 7; i++)
				{
					SetLineSpecial(1788+i, Floor_RaiseToNearest, 100, 32);
					SetLineActivation(1788+i, SPAC_Cross);
				}
				SetLineSpecial(1796, Floor_RaiseToNearest, 100, 32);
				SetLineActivation(1796, SPAC_Cross);
				SetLineSpecial(1800, Floor_RaiseToNearest, 100, 32);
				SetLineActivation(1800, SPAC_Cross);
				SetLineSpecial(1802, Floor_RaiseToNearest, 100, 32);
				SetLineActivation(1802, SPAC_Cross);
				ClearSectorTags(412);
				AddSectorTag(412, 100);
				// Shootable cross at demon-faced floor changed to repeatable
				// action to prevent softlock if "spikes" are raised again
				SetLineFlags(2600, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '3C10B1B017E902BE7CDBF2436DF56973': // Requiem MAP08
			{
				// Flag deathmatch soulsphere for 100% items in single-player
				SetThingFlags(48, 17);
				// Allow player to leave inescapable lift using the walls
				for(int i = 0; i < 3; i++)
				{
					SetLineSpecial(2485+i, Plat_DownWaitUpStayLip, 68, 64, 105, 0);
					SetLineActivation(2485+i, SPAC_Use);
					SetLineFlags(2485+i, Line.ML_REPEAT_SPECIAL);
				}
				SetLineSpecial(848, Plat_DownWaitUpStayLip, 68, 64, 105, 0);
				SetLineActivation(848, SPAC_UseBack);
				SetLineFlags(848, Line.ML_REPEAT_SPECIAL);
				SetLineSpecial(895, Plat_DownWaitUpStayLip, 68, 64, 105, 0);
				SetLineActivation(895, SPAC_UseBack);
				SetLineFlags(895, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '14FE46ED0458979118007E1906A0C9BC': // Requiem MAP09
			{
				// Flag deathmatch items for 100% items in single-player
				for(int i = 0; i < 6; i++)
					SetThingFlags(371+i, 17);
				
				for(int i = 0; i < 19; i++)
					SetThingFlags(402+i, 17);
				
				SetThingFlags(359, 17);
				SetThingFlags(389, 17);
				SetThingFlags(390, 17);
				// Change sides of blue skull platform to be repeatable
				for(int i = 0; i < 4; i++)
					SetLineFlags(873+i, Line.ML_REPEAT_SPECIAL);
				// Make switch that raises bars near yellow skull repeatable
				SetLineFlags(2719, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case '53A6369C3C8DA4E7AC443A8F8684E38E': // Requiem MAP12
			{
				// Remove unreachable secrets
				SetSectorSpecial(352, 0);
				SetSectorSpecial(503, 0);
				// Change action of eastern switch at pool of water so that it
				// lowers the floor properly, making the secret accessible
				SetLineSpecial(4873, Floor_LowerToLowest, 62, 8);
				break;
			}
			
			case '2DAB6E4B19B4F2763695267D39CD0275': // Requiem MAP13
			{
				// Fix missing nukage at starting bridge on hardware renderer
				for(int i = 0; i < 4; i++)
					SetLineSectorRef(2152+i, Line.back, 8);
				break;
			}
			
			case 'F55FB2A8DC68CFC75E4340EF4ED7A8BF': // Requiem MAP21
			{
				// Fix self-referencing floor hack
				for(int i = 0; i < 4; i++)
					SetLineSectorRef(3+i, Line.back, 219);
					
				SetLineSpecial(8, Transfer_Heights, 80);
				// Fix south side of pit hack so textures don't bleed through
				// the fake floor on hardware renderer
				SetLineSectorRef(267, Line.back, 63);
				SetLineSectorRef(268, Line.back, 63);
				SetLineSectorRef(270, Line.back, 63);
				SetLineSectorRef(271, Line.back, 63);
				SetLineSectorRef(274, Line.back, 63);
				SetLineSectorRef(275, Line.back, 63);
				SetLineSectorRef(3989, Line.back, 63);
				SetLineSectorRef(3994, Line.back, 63);
				// Fix fake 3D bridge on hardware renderer
				SetLineSectorRef(506, Line.back, 841);
				SetLineSectorRef(507, Line.back, 841);
				SetLineSectorRef(536, Line.back, 841);
				SetLineSectorRef(537, Line.back, 841);
				SetLineSectorRef(541, Line.back, 841);
				SetLineSectorRef(547, Line.back, 841);
				AddSectorTag(90, 1000);
				AddSectorTag(91, 1000);
				SetSectorTexture(90, Sector.floor, "MFLR8_4");
				SetSectorTexture(91, Sector.floor, "MFLR8_4");
				
				SetLineSectorRef(553, Line.back, 841);
				SetLineSectorRef(554, Line.back, 841);
				SetLineSectorRef(559, Line.back, 841);
				SetLineSectorRef(560, Line.back, 841);
				SetLineSectorRef(562, Line.back, 841);
				SetLineSectorRef(568, Line.back, 841);
				AddSectorTag(96, 1000);
				AddSectorTag(97, 1000);
				SetSectorTexture(96, Sector.floor, "MFLR8_4");
				SetSectorTexture(97, Sector.floor, "MFLR8_4");
				SetLineSpecial(505, Transfer_Heights, 1000);
				// Fix randomly appearing ceiling at deep water
				SetLineSectorRef(1219, Line.front, 233);
				SetLineSectorRef(1222, Line.front, 233);
				SetLineSectorRef(1223, Line.front, 233);
				SetLineSectorRef(1228, Line.front, 233);
				// Make switch in sky room repeatable so player does not get
				// trapped at red cross if returning a second time
				SetLineFlags(3870, Line.ML_REPEAT_SPECIAL);
				// Move unreachable item bonuses
				SetThingXY(412, -112, 6768);
				SetThingXY(413, -112, 6928);
				SetThingXY(414, -96, 6928);
				SetThingXY(415, -96, 6768);
				// Remove unreachable secret at exit megasphere
				SetSectorSpecial(123, 0);
				break;
			}
			
			case '2499CF9A9351BE9BC4E9C66FC9F291A7': // Requiem MAP23
			{
				// Have arch-vile who creates ghost monsters not count as a kill
				SetThingFlags(0, GetThingFlags(0) | MTF_NOCOUNT);
				// Remove secret at switch that can only be scored by crouching
				SetSectorSpecial(240, 0);
				break;
			}

			case '1497894956B3C8EBE8A240B7FDD99C6A': // Memento Mori 2 MAP25
			{
				// an imp is used for the lift activation and cannot be killed
				SetThingFlags(51, GetThingFlags (51) | MTF_NOCOUNT);
				break;
			}

			case '51960F3E9D46449E98DBC7D97F49DB23': // Shotgun Symphony E1M1
			{
				// harmless cyberdemon included for the 'story' sake
				SetThingFlags(158, GetThingFlags (158) | MTF_NOCOUNT);
				break;
			}

			case '60D362BAE16B4C10A1DCEE442C878CAE': // 50 Shades of Graytall MAP06
			{
				// there are four invisibility spheres used for decoration
				SetThingFlags(144, GetThingFlags (144) | MTF_NOCOUNT);
				SetThingFlags(145, GetThingFlags (145) | MTF_NOCOUNT);
				SetThingFlags(194, GetThingFlags (194) | MTF_NOCOUNT);
				SetThingFlags(195, GetThingFlags (195) | MTF_NOCOUNT);
				break;
			}

			case 'C104E740CC3F70BCFD5D2EA8E833318D': // 50 Monsters MAP29
			{
				// there are two invisibility spheres used for decoration
				SetThingFlags(111, GetThingFlags (111) | MTF_NOCOUNT);
				SetThingFlags(112, GetThingFlags (112) | MTF_NOCOUNT);
				break;
			}

			case '76393C84102480A4C75A4674C9C3217A': // Deadly Standards 2 E2M8
			{
				// 923 lost souls are used as environmental hazard
				for (int i = 267; i < 275; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 482; i < 491; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 510; i < 522; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 880; i < 1510; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 1622; i < 1660; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);

				for (int i = 1682; i < 1820; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 1847; i < 1875; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 2110; i < 2114; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 2243; i < 2293; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);

				SetThingFlags(493,  GetThingFlags (493)  | MTF_NOCOUNT);
				SetThingFlags(573,  GetThingFlags (573)  | MTF_NOCOUNT);
				SetThingFlags(613,  GetThingFlags (613)  | MTF_NOCOUNT);
				SetThingFlags(614,  GetThingFlags (614)  | MTF_NOCOUNT);
				SetThingFlags(1679, GetThingFlags (1679) | MTF_NOCOUNT);
				SetThingFlags(1680, GetThingFlags (1680) | MTF_NOCOUNT);
				break;
			}

			case '42B4D294A60BE4E3500AF150291CF6D4': // Hell Ground MAP05
			{
				// 5 cyberdemons are located at the 'pain end' sector
				for (int i = 523; i < 528; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				break;
			}

			case '0C0513A9821F26F3D7997E3B0359A318': // Mayhem 1500 MAP06
			{
				// there's an archvile behind the bossbrain at the very end that can't be killed
				SetThingFlags(61, GetThingFlags (61) | MTF_NOCOUNT);
				break;
			}

			case '00641DA23DDE998F6725BC5896A0DBC2': // 20 Years of Doom E1M8
			{
				// 32 lost souls are located at the 'pain end' sector
				for (int i = 1965; i < 1975; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 2189; i < 2202; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 2311; i < 2320; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				break;
			}

			case 'EEBDD9CA280F6FF06C30AF2BEE85BF5F': // 2002ad10.wad E3M3
			{
				// swarm of cacodemons at the end meant to be pseudo-endless and not killed
				for (int i = 467; i < 547; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				break;
			}

			case '988DFF5BB7073B857DEE3957A91C8518': // Speed of Doom MAP14
			{
				// you can get only one of the soulspheres, the other, depending on your choice, becomes unavailable
				SetThingFlags(1044, GetThingFlags (1044) | MTF_NOCOUNT);			
				SetThingFlags(1045, GetThingFlags (1045) | MTF_NOCOUNT);
				break;
			}

			case '361734AC5D78E872A05335C83E4F6DB8': // inf-lutz.wad E3M8
			{
				// there is a trap with 10 cyberdemons at the end of the map, you are not meant to kill them
				for (int i = 541; i < 546; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				for (int i = 638; i < 643; i++)
					SetThingFlags (i, GetThingFlags (i) | MTF_NOCOUNT);
				break;
			}

			case '8F844B272E7235E82EA78AD2A2EB2D4A': // Serenity E3M7
			{
				// two spheres can't be obtained and thus should not count towards 100% items
				SetThingFlags(443, GetThingFlags (443) | MTF_NOCOUNT);
				SetThingFlags(444, GetThingFlags (444) | MTF_NOCOUNT);
				// one secret is unobtainable
				SetSectorSpecial (97, 0);
				break;
			}

			case 'A50AC05CCE4F07413A0C4883C5E24215': // dbimpact.wad e1m7
			{
				SetLineFlags(1461, Line.ML_REPEAT_SPECIAL);
				SetLineFlags(1468, Line.ML_REPEAT_SPECIAL);
				break;
			}
			
			case 'E0D747B9EE58A0CB74B9AD54423AC15C': // return01.wad e1m2
			{
				// fix broken switch to raise the exit bridge
				SetLineSpecial(1248, Floor_RaiseByValue, 39, 8, 512);
				break;
			}

			case '1C35384B22BD805F51B3B2C9D17D62E4': // 007ltsd.wad E4M7
			{
				// Fix impassable exit line
				SetLineFlags(6842, 0, Line.ML_BLOCKING); 
				break;
			}
			
			case '50E394239FF64264950D11883E933553': // 1024.wad map05
			{
				// Change duplicate player 2 start to player 3 start
				SetThingEdNum(59, 3);
				break;
			}

			case '3F0965ADCEB2F4A7BF46FADF6DD941B0': // phocas2.wad map01
			{
				// turn map spot into teleport dest.
				SetThingEdNum(699, 9044);
				break;
			}
			
			case 'C8E727FFBA0BA445666C80340BF3D0AC': // god_.WAD E1M2
			{
				// fix bad skill flags for a monster that's required to be killed.
				SetThingSkills(1184, 1);
				break;
			}

			case '3B4AAD34E46443BD505CC6053FCD842A': // pc_cp2.wad map38
			{
				// Emulate the effect of the hidden Commander Keen's death
				// since the Keen actor is modified by DEHACKED and doesn't work as intended:

				// 1) Replace the Keen with a zombieman
				SetThingEdNum(101, 3004);

				// 2) Set its special to emulate A_KeenDie
				SetThingSpecial(101, Door_Open);
				SetThingArgument(101, 0, 666);
				SetThingArgument(101, 1, 16);
			}
		}
	}
}
