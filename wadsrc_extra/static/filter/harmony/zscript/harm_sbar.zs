class HarmonyStatusBar : DoomStatusBar
{
	override void DrawBarAmmo()
	{
		int amt1, maxamt;
		[amt1, maxamt] = GetAmount("MinigunAmmo");
		DrawString(mIndexFont, FormatNumber(amt1, 3), (288, 173), DI_TEXT_ALIGN_RIGHT);
		DrawString(mIndexFont, FormatNumber(maxamt, 3), (314, 173), DI_TEXT_ALIGN_RIGHT);
		
		[amt1, maxamt] = GetAmount("Shells");
		DrawString(mIndexFont, FormatNumber(amt1, 3), (288, 179), DI_TEXT_ALIGN_RIGHT);
		DrawString(mIndexFont, FormatNumber(maxamt, 3), (314, 179), DI_TEXT_ALIGN_RIGHT);

		[amt1, maxamt] = GetAmount("TimeBombAmmo");
		DrawString(mIndexFont, FormatNumber(amt1, 3), (288, 185), DI_TEXT_ALIGN_RIGHT);
		DrawString(mIndexFont, FormatNumber(maxamt, 3), (314, 185), DI_TEXT_ALIGN_RIGHT);
		
		[amt1, maxamt] = GetAmount("ChaosBarsAmmo");
		DrawString(mIndexFont, FormatNumber(amt1, 3), (288, 191), DI_TEXT_ALIGN_RIGHT);
		DrawString(mIndexFont, FormatNumber(maxamt, 3), (314, 191), DI_TEXT_ALIGN_RIGHT);
	}
}
