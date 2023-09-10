/******************************************************************************
 * 
 * Light coronas! (like in a certain un-real game...)
 * 
 * HOW TO USE:
 * 
 * Make a new actor inheriting from Corona, then create a Spawn state. Place
 * the actor in the map like you normally would any other decoration. The game
 * will draw the Spawn state's sprite in screen space.
 * 
 * The corona will be drawn with the actor's render style and alpha. It
 * defaults to the "Add" RenderStyle, but can be changed to any render style.
 * 
 * The actor's scale can also be used to influence the size of the corona
 * on screen.
 * 
 * The RenderRadius property can be used to set the maximum distance a corona
 * will be visible before it completely fades out. This also helps improve
 * performance - any coronas beyond the RenderRadius will not be processed
 * by the engine.
 * 
 * Currently, solid geometry and other solid actors will occlude the coronas.
 * Textures with transparent pixels do not. Also, coronas don't work across
 * portals (that includes skyboxes and mirrors). Calling for help - anyone
 * who'd like to help implement portal support, please send a pull request. :)
 * 
 *****************************************************************************/

class Corona : Actor abstract native
{
	Default
	{
		RenderStyle "Add";
		RenderRadius 1024.0;
		+NOINTERACTION
		+NOGRAVITY
		+FORCEXYBILLBOARD
	}
}
