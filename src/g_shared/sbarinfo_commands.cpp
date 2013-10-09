/*
** sbarinfo_commands.cpp
**
** This is an extension to the sbarinfo.cpp file.  This contains all of the
** commands.
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// [BL] Do note that this file is included by sbarinfo.cpp.  This is done so
//      that all the code can be more or less in one spot.
//
//      At the bottom of this file is a function that belongs to
//      SBarInfoCommandFlowControl which creates the instances of the following
//      classes.
////////////////////////////////////////////////////////////////////////////////

class CommandDrawImage : public SBarInfoCommandFlowControl
{
	public:
		CommandDrawImage(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			translatable(false), type(NORMAL_IMAGE), image(-1), maxwidth(-1),
			maxheight(-1), spawnScaleX(1.0f), spawnScaleY(1.0f), flags(0),
			applyscale(false), offset(static_cast<Offset> (TOP|LEFT)),
			texture(NULL), alpha(FRACUNIT)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			if(flags & DI_ALTERNATEONFAIL)
				SBarInfoCommandFlowControl::Draw(block, statusBar);

			if(texture == NULL)
				return;

			int w = maxwidth, h = maxheight;
			
			// We must calculate this per frame in order to prevent glitches with cl_capfps true.
			fixed_t frameAlpha = block->Alpha();
			if(alpha != FRACUNIT)
				frameAlpha = fixed_t(((double) block->Alpha() / (double) FRACUNIT) * ((double) alpha / (double) OPAQUE) * FRACUNIT);
			
			if(flags & DI_DRAWINBOX)
			{
				double scale1, scale2;
				scale1 = scale2 = 1.0f;
				double texwidth = (int) (texture->GetScaledWidthDouble()*spawnScaleX);
				double texheight = (int) (texture->GetScaledHeightDouble()*spawnScaleY);
				
				if (w != -1 && (w<texwidth || (flags & DI_FORCESCALE)))
				{
					scale1 = w/texwidth;
				}
				if (h != -1 && (h<texheight || (flags & DI_FORCESCALE)))
				{
					scale2 = h/texheight;
				}
				
				if (flags & DI_FORCESCALE)
				{
					if (w == -1 || (h != -1 && scale2<scale1))
						scale1=scale2;
				}
				else if (scale2<scale1) scale1=scale2;
				
				w=(int)(texwidth*scale1);
				h=(int)(texheight*scale1);
			}
			else if (applyscale)
			{
				w=(int) (texture->GetScaledWidthDouble()*spawnScaleX);
				h=(int) (texture->GetScaledHeightDouble()*spawnScaleY);
			}
			statusBar->DrawGraphic(texture, imgx, imgy, block->XOffset(), block->YOffset(), frameAlpha, block->FullScreenOffsets(),
				translatable, false, offset, false, w, h);
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			bool parenthesized = false;
			bool getImage = true;
			if(sc.CheckToken(TK_Identifier))
			{
				getImage = false;
				if(sc.Compare("playericon"))
					type = PLAYERICON;
				else if(sc.Compare("ammoicon1"))
					type = AMMO1;
				else if(sc.Compare("ammoicon2"))
					type = AMMO2;
				else if(sc.Compare("armoricon"))
					type = ARMOR;
				else if(sc.Compare("weaponicon"))
					type = WEAPONICON;
				else if(sc.Compare("sigil"))
					type = SIGIL;
				else if(sc.Compare("hexenarmor"))
				{
					parenthesized = sc.CheckToken('(');

					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("armor"))
						type = HEXENARMOR_ARMOR;
					else if(sc.Compare("shield"))
						type = HEXENARMOR_SHIELD;
					else if(sc.Compare("helm"))
						type = HEXENARMOR_HELM;
					else if(sc.Compare("amulet"))
						type = HEXENARMOR_AMULET;
					else
					{
						sc.ScriptMessage("Unknown armor type: '%s'", sc.String);
						type = HEXENARMOR_ARMOR;
					}
					sc.MustGetToken(',');
					getImage = true;
				}
				else if(sc.Compare("translatable"))
				{
					translatable = true;
					getImage = true;
				}
				else
				{
					type = INVENTORYICON;
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
					}
					else
					{
						sprite = ((AInventory *)GetDefaultByType(item))->Icon;
					}
					image = -1;
				}
			}
			if(getImage)
			{
				sc.MustGetToken(TK_StringConst);
				image = script->newImage(sc.String);
				sprite.SetInvalid();

				if(parenthesized) sc.MustGetToken(')');
			}
			sc.MustGetToken(',');
			GetCoordinates(sc, fullScreenOffsets, imgx, imgy);
			if(sc.CheckToken(','))
			{
				// Use none instead of topleft in case we decide we want to use
				// alignments to remove the offset from images.
				if(!sc.CheckToken(TK_None))
				{
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("center"))
						offset = CENTER;
					else if(sc.Compare("centerbottom"))
						offset = static_cast<Offset> (HMIDDLE|BOTTOM);
					else
						sc.ScriptError("'%s' is not a valid alignment.", sc.String);
				}
			}
			if(sc.CheckToken(','))
			{
				sc.MustGetToken(TK_IntConst);
				if((maxwidth = sc.Number) > 0)
					flags |= DI_DRAWINBOX;
				else
					maxwidth = -1;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				if ((maxheight = sc.Number) > 0)
					flags |= DI_DRAWINBOX;
				else
					maxheight = -1;
			}
			if(sc.CheckToken(','))
			{
				while(sc.CheckToken(TK_Identifier))
				{
					if(sc.Compare("skipicon"))
						flags |= DI_SKIPICON;
					else if(sc.Compare("skipalticon"))
						flags |= DI_SKIPALTICON;
					else if(sc.Compare("skipspawn"))
						flags |= DI_SKIPSPAWN;
					else if(sc.Compare("skipready"))
						flags |= DI_SKIPREADY;
					else if(sc.Compare("alticonfirst"))
						flags |= DI_ALTICONFIRST;
					else if(sc.Compare("forcescale"))
					{
						if(flags & DI_DRAWINBOX)
							flags |= DI_FORCESCALE;
					}
					else if(sc.Compare("alternateonfail"))
						flags |= DI_ALTERNATEONFAIL;
					else
						sc.ScriptError("Unknown flag '%s'.", sc.String);
					if(!sc.CheckToken('|') && !sc.CheckToken(',')) break;
				}
			}
			if(flags & DI_ALTERNATEONFAIL)
				SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
			else
				sc.MustGetToken(';');
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			texture = NULL;
			alpha = FRACUNIT;
			if (applyscale)
			{
				spawnScaleX = spawnScaleY = 1.0f;
				applyscale = false;
			}
			if(type == PLAYERICON)
				texture = TexMan[statusBar->CPlayer->mo->ScoreIcon];
			else if(type == AMMO1)
			{
				AAmmo *ammo = statusBar->ammo1;
				if(ammo != NULL)
					GetIcon(ammo);
			}
			else if(type == AMMO2)
			{
				AAmmo *ammo = statusBar->ammo2;
				if(ammo != NULL)
					GetIcon(ammo);
			}
			else if(type == ARMOR)
			{
				ABasicArmor *armor = statusBar->armor;
				if(armor != NULL && armor->Amount != 0)
					GetIcon(armor);
			}
			else if(type == WEAPONICON)
			{
				AWeapon *weapon = statusBar->CPlayer->ReadyWeapon;
				if(weapon != NULL)
					GetIcon(weapon);
			}
			else if(type == SIGIL)
			{
				AInventory *item = statusBar->CPlayer->mo->FindInventory<ASigil>();
				if (item != NULL)
					texture = TexMan[item->Icon];
			}
			else if(type == HEXENARMOR_ARMOR || type == HEXENARMOR_SHIELD || type == HEXENARMOR_HELM || type == HEXENARMOR_AMULET)
			{
				int armorType = type - HEXENARMOR_ARMOR;
			
				AHexenArmor *harmor = statusBar->CPlayer->mo->FindInventory<AHexenArmor>();
				if (harmor != NULL)
				{
					if (harmor->Slots[armorType] > 0 && harmor->SlotsIncrement[armorType] > 0)
					{
						//combine the alpha values
						alpha = fixed_t(((double) alpha / (double) FRACUNIT) * ((double) MIN<fixed_t> (OPAQUE, Scale(harmor->Slots[armorType], OPAQUE, harmor->SlotsIncrement[armorType])) / (double) OPAQUE) * FRACUNIT);
						texture = statusBar->Images[image];
					}
					else
						return;
				}
			}
			else if(type == INVENTORYICON)
				texture = TexMan[sprite];
			else if(type == SELECTEDINVENTORYICON && statusBar->CPlayer->mo->InvSel != NULL)
				texture = TexMan(statusBar->CPlayer->mo->InvSel->Icon);
			else if(image >= 0)
				texture = statusBar->Images[image];
			
			if (flags & DI_ALTERNATEONFAIL)
			{
				SetTruth(texture == NULL || !(texture->GetID().isValid()), block, statusBar);
			}
		}
	protected:
		void	GetIcon(AInventory *item)
		{
			FTextureID icon = GetInventoryIcon(item, flags, &applyscale);
			
			if (applyscale)
			{
				spawnScaleX = FIXED2FLOAT(item->scaleX);
				spawnScaleY = FIXED2FLOAT(item->scaleY);
			}
			
			texture = TexMan[icon];
		}
		
		enum ImageType
		{
			PLAYERICON,
			AMMO1,
			AMMO2,
			ARMOR,
			WEAPONICON,
			SIGIL,
			HEXENARMOR_ARMOR,
			HEXENARMOR_SHIELD,
			HEXENARMOR_HELM,
			HEXENARMOR_AMULET,
			INVENTORYICON,
			WEAPONSLOT,
			SELECTEDINVENTORYICON,

			NORMAL_IMAGE
		};

		bool				translatable;
		ImageType			type;
		int					image;
		FTextureID			sprite;
		int					maxwidth;
		int					maxheight;
		double				spawnScaleX;
		double				spawnScaleY;
		DWORD				flags;
		bool				applyscale; //Set remotely from from GetInventoryIcon when selected sprite comes from Spawn state
		// I'm using imgx/imgy here so that I can inherit drawimage with drawnumber for some commands.
		SBarInfoCoordinate	imgx;
		SBarInfoCoordinate	imgy;
		Offset				offset;

		FTexture			*texture;
		int					alpha;
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawSwitchableImage : public CommandDrawImage
{
	private:
		enum Operator
		{
			EQUAL,
			LESS,
			GREATER,
			LESSOREQUAL,
			GREATEROREQUAL,
			NOTEQUAL
		};

		static void	GetOperation(FScanner &sc, Operator &op, int &value)
		{
			if(sc.CheckToken(TK_Eq))
				op = EQUAL;
			else if(sc.CheckToken('<'))
				op = LESS;
			else if(sc.CheckToken('>'))
				op = GREATER;
			else if(sc.CheckToken(TK_Leq))
				op = LESSOREQUAL;
			else if(sc.CheckToken(TK_Geq))
				op = GREATEROREQUAL;
			else if(sc.CheckToken(TK_Neq))
				op = NOTEQUAL;
			else
			{ // Default
				op = GREATER;
				value = 0;
				return;
			}
			sc.MustGetToken(TK_IntConst);
			value = sc.Number;
		}
		static bool	EvaluateOperation(const Operator &op, const int &value, const int &compare)
		{
			switch(op)
			{
				case EQUAL:
					return compare == value;
				case LESS:
					return compare < value;
				case GREATER:
				default:
					return compare > value;
				case LESSOREQUAL:
					return compare <= value;
				case GREATEROREQUAL:
					return compare >= value;
				case NOTEQUAL:
					return compare != value;
			}
		}

	public:
		CommandDrawSwitchableImage(SBarInfo *script) : CommandDrawImage(script),
			condition(INVENTORY), conditionAnd(false)
		{
			conditionalImage[0] = conditionalImage[1] = conditionalImage[2] = -1;
			conditionalValue[0] = conditionalValue[1] = 0;
			armorType[0] = armorType[1] = 0;
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			if(sc.TokenType == TK_Identifier)
			{
				if(sc.Compare("weaponslot"))
				{
					condition = WEAPONSLOT;
					sc.MustGetToken(TK_IntConst);
					conditionalValue[0] = sc.Number;
				}
				else if(sc.Compare("invulnerable"))
				{
					condition = INVULNERABILITY;
				}
				else if(sc.Compare("keyslot"))
				{
					condition = KEYSLOT;
					sc.MustGetToken(TK_IntConst);
					conditionalValue[0] = sc.Number;
				}
				else if(sc.Compare("armortype"))
				{
					condition = ARMORTYPE;
					sc.MustGetToken(TK_Identifier);
					armorType[0] = FName(sc.String).GetIndex();
					GetOperation(sc, conditionalOperator[0], conditionalValue[0]);
				}
			}
			if(condition == INVENTORY)
			{
				inventoryItem[0] = sc.String;
				const PClass* item = PClass::FindClass(sc.String);
				if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
				{
					sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
				}
				GetOperation(sc, conditionalOperator[0], conditionalValue[0]);
			}
			if(sc.CheckToken(TK_AndAnd) && condition != INVULNERABILITY)
			{
				conditionAnd = true;
				if(condition == KEYSLOT || condition == WEAPONSLOT)
				{
					sc.MustGetToken(TK_IntConst);
					conditionalValue[1] = sc.Number;
				}
				else if(condition == ARMORTYPE)
				{
					sc.MustGetToken(TK_Identifier);
					armorType[1] = FName(sc.String).GetIndex();
					GetOperation(sc, conditionalOperator[1], conditionalValue[1]);
				}
				else
				{
					sc.MustGetToken(TK_Identifier);
					inventoryItem[1] = sc.String;
					const PClass* item = PClass::FindClass(sc.String);
					if(item == NULL || !PClass::FindClass("Inventory")->IsAncestorOf(item)) //must be a kind of Inventory
					{
						sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
					}
					GetOperation(sc, conditionalOperator[1], conditionalValue[1]);
				}
			}
			// [BL] I have word that MSVC++ wants this static_cast ;)  Shut up MSVC!
			for(unsigned int i = 0;i < (conditionAnd ? 3u : 1u);i++)
			{
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				conditionalImage[i] = script->newImage(sc.String);
			}
			sc.MustGetToken(',');
			CommandDrawImage::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			// DrawSwitchable image allows 2 or 4 images to be supplied.
			// drawAlt toggles these:
			// 1 = first image
			// 2 = second image
			// 3 = thrid image
			// 0 = last image
			int drawAlt = 0;
			if(condition == WEAPONSLOT) //weaponslots
			{
				drawAlt = 1; //draw off state until we know we have something.
				for (int i = 0; i < statusBar->CPlayer->weapons.Slots[conditionalValue[0]].Size(); i++)
				{
					const PClass *weap = statusBar->CPlayer->weapons.Slots[conditionalValue[0]].GetWeapon(i);
					if(weap == NULL)
					{
						continue;
					}
					else if(statusBar->CPlayer->mo->FindInventory(weap) != NULL)
					{
						drawAlt = 0;
						break;
					}
				}
			}
			else if(condition == INVULNERABILITY)
			{
				if(statusBar->CPlayer->cheats&CF_GODMODE)
				{
					drawAlt = 1;
				}
			}
			else if(condition == KEYSLOT)
			{
				bool found1 = false;
				bool found2 = false;
				drawAlt = 1;

				for(AInventory *item = statusBar->CPlayer->mo->Inventory;item != NULL;item = item->Inventory)
				{
					if(item->IsKindOf(RUNTIME_CLASS(AKey)))
					{
						int keynum = static_cast<AKey *>(item)->KeyNumber;
		
						if(keynum == conditionalValue[0])
							found1 = true;
						if(conditionAnd && keynum == conditionalValue[1]) // two keys
							found2 = true;
					}
				}

				if(conditionAnd)
				{
					if(found1 && found2)
						drawAlt = 0;
					else if(found1)
						drawAlt = 2;
					else if(found2)
						drawAlt = 3;
				}
				else
				{
					if(found1)
						drawAlt = 0;
				}
			}
			else if(condition == ARMORTYPE)
			{
				ABasicArmor *armor = (ABasicArmor *) statusBar->CPlayer->mo->FindInventory(NAME_BasicArmor);
				if(armor != NULL)
				{
					bool matches1 = armor->ArmorType.GetIndex() == armorType[0] && EvaluateOperation(conditionalOperator[0], conditionalValue[0], armor->Amount);
					bool matches2 = armor->ArmorType.GetIndex() == armorType[1] && EvaluateOperation(conditionalOperator[1], conditionalValue[1], armor->Amount);

					drawAlt = 1;
					if(conditionAnd)
					{
						if(matches1 && matches2)
							drawAlt = 0;
						else if(matches2)
							drawAlt = 3;
						else if(matches1)
							drawAlt = 2;
					}
					else if(matches1)
						drawAlt = 0;
				}
			}
			else //check the inventory items and draw selected sprite
			{
				AInventory* item = statusBar->CPlayer->mo->FindInventory(PClass::FindClass(inventoryItem[0]));
				if(item == NULL || !EvaluateOperation(conditionalOperator[0], conditionalValue[0], item->Amount))
					drawAlt = 1;
				if(conditionAnd)
				{
					item = statusBar->CPlayer->mo->FindInventory(PClass::FindClass(inventoryItem[1]));
					bool secondCondition = item != NULL && EvaluateOperation(conditionalOperator[1], conditionalValue[1], item->Amount);
					if((item != NULL && secondCondition) && drawAlt == 0) //both
					{
						drawAlt = 0;
					}
					else if((item != NULL && secondCondition) && drawAlt == 1) //2nd
					{
						drawAlt = 3;
					}
					else if((item == NULL || !secondCondition) && drawAlt == 0) //1st
					{
						drawAlt = 2;
					}
				}
			}
			if(drawAlt != 0) //draw 'off' image
			{
				texture = statusBar->Images[conditionalImage[drawAlt-1]];

				// Since we're not going to call our parent's tick() method,
				// be sure to set the alpha value properly.
				alpha = FRACUNIT;
				return;
			}
			CommandDrawImage::Tick(block, statusBar, hudChanged);
		}

	private:
		enum Condition
		{
			WEAPONSLOT,
			INVULNERABILITY,
			KEYSLOT,
			ARMORTYPE,

			INVENTORY
		};

		Condition	condition;
		bool		conditionAnd;
		int			conditionalImage[3];
		int			conditionalValue[2];
		Operator	conditionalOperator[2];
		FString		inventoryItem[2];
		int			armorType[2];
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawString : public SBarInfoCommand
{
	public:
		CommandDrawString(SBarInfo *script) : SBarInfoCommand(script),
			lineBreaks(false), breakWidth(320), shadow(false), shadowX(2),
			shadowY(2), spacing(0), font(NULL), translation(CR_UNTRANSLATED),
			cache(-1), strValue(CONSTANT), valueArgument(0), alignment(ALIGN_RIGHT)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			if(lineBreaks)
			{
				FBrokenLines *lines = V_BreakLines(font, breakWidth, str.GetChars());
				for(int i = 0;lines[i].Width >= 0;i++)
				{
					statusBar->DrawString(font, lines[i].Text, x, y+i*(font->GetHeight()+4), block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), translation, spacing, shadow, shadowX, shadowY);
				}
				V_FreeBrokenLines(lines);
			}
			else
				statusBar->DrawString(font, str.GetChars(), x, y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), translation, spacing, shadow, shadowX, shadowY);
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			font = V_GetFont(sc.String);
			if(font == NULL)
			{
				sc.ScriptMessage("Unknown font '%s'.", sc.String);
				font = SmallFont;
			}
			sc.MustGetToken(',');
			translation = GetTranslation(sc);
			sc.MustGetToken(',');
			ParseStringValue(sc);
			sc.MustGetToken(',');
			GetCoordinates(sc, fullScreenOffsets, startX, y);
			if(sc.CheckToken(',')) //spacing
			{
				sc.MustGetToken(TK_IntConst);
				spacing = sc.Number;
				if(sc.CheckToken(',')) //[KS] flags? flags! SIX FLAGS!
				{
					while(sc.CheckToken(TK_Identifier))
					{
						if(sc.Compare("alignment"))
						{
							sc.MustGetToken('(');
							sc.MustGetToken(TK_Identifier);
							if(sc.Compare("right"))
								alignment = ALIGN_RIGHT;
							else if(sc.Compare("left"))
								alignment = ALIGN_LEFT;
							else if(sc.Compare("center"))
								alignment = ALIGN_CENTER;
							else
								sc.ScriptError("Unknown alignment '%s'.", sc.String);
							sc.MustGetToken(')');
						}
						else if(sc.Compare("drawshadow"))
						{
							if(sc.CheckToken('('))
							{
								sc.MustGetToken(TK_IntConst);
								shadowX = sc.Number;
								sc.MustGetToken(',');
								sc.MustGetToken(TK_IntConst);
								shadowY = sc.Number;
								sc.MustGetToken(')');
							}
							shadow = true;
						}
						else if(sc.Compare("linebreaks"))
						{
							sc.MustGetToken('(');
							sc.MustGetToken(TK_IntConst);
							breakWidth = sc.Number;
							sc.MustGetToken(')');
							lineBreaks = true;
						}
						else
							sc.ScriptError("Unknown flag '%s'.", sc.String);
						if(!sc.CheckToken('|') && !sc.CheckToken(',')) break;
					}
				}
			}
			sc.MustGetToken(';');

			RealignString();
		}
		void	ParseStringValue(FScanner &sc)
		{
			if(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("levelname"))
					strValue = LEVELNAME;
				else if(sc.Compare("levellump"))
					strValue = LEVELLUMP;
				else if(sc.Compare("skillname"))
					strValue = SKILLNAME;
				else if(sc.Compare("playerclass"))
					strValue = PLAYERCLASS;
				else if(sc.Compare("playername"))
					strValue = PLAYERNAME;
				else if(sc.Compare("ammo1tag"))
					strValue = AMMO1TAG;
				else if(sc.Compare("ammo2tag"))
					strValue = AMMO2TAG;
				else if(sc.Compare("weapontag"))
					strValue = WEAPONTAG;
				else if(sc.Compare("inventorytag"))
					strValue = INVENTORYTAG;
				else if(sc.Compare("time"))
					strValue = TIME;
				else if(sc.Compare("logtext"))
					strValue = LOGTEXT;
				else if(sc.Compare("globalvar"))
				{
					bool parenthesized = sc.CheckToken('(');

					strValue = GLOBALVAR;
					sc.MustGetToken(TK_IntConst);
					if(sc.Number < 0 || sc.Number >= NUM_GLOBALVARS)
						sc.ScriptError("Global variable number out of range: %d", sc.Number);
					valueArgument = sc.Number;

					if(parenthesized) sc.MustGetToken(')');
				}
				else if(sc.Compare("globalarray"))
				{
					bool parenthesized = sc.CheckToken('(');

					strValue = GLOBALARRAY;
					sc.MustGetToken(TK_IntConst);
					if(sc.Number < 0 || sc.Number >= NUM_GLOBALVARS)
						sc.ScriptError("Global variable number out of range: %d", sc.Number);
					valueArgument = sc.Number;

					if(parenthesized) sc.MustGetToken(')');
				}
				else
					sc.ScriptError("Unknown string '%s'.", sc.String);
			}
			else
			{
				strValue = CONSTANT;
				sc.MustGetToken(TK_StringConst);
				if(sc.String[0] == '$')
					str = GStrings[sc.String+1];
				else
					str = sc.String;
			}
		}
		void	Reset()
		{
			switch(strValue)
			{
				case PLAYERCLASS:
					// userinfo changes before the actual class change.
				case SKILLNAME:
					// Although it's not possible for the skill level to change
					// midlevel, it is possible the level was restarted.
					cache = -1;
					break;
				default:
					break;
			}
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			switch(strValue)
			{
				case LEVELNAME:
					if(level.lumpnum != cache)
					{
						cache = level.lumpnum;
						str = level.LevelName;
						RealignString();
					}
					break;
				case LEVELLUMP:
					if(level.lumpnum != cache)
					{
						cache = level.lumpnum;
						str = level.mapname;
						str.ToUpper();
						RealignString();
					}
					break;
				case SKILLNAME:
					if(level.lumpnum != cache) // Can only change skill between level.
					{
						cache = level.lumpnum;
						str = G_SkillName();
						RealignString();
					}
					break;
				case PLAYERCLASS:
					if(statusBar->CPlayer->userinfo.GetPlayerClassNum() != cache)
					{
						cache = statusBar->CPlayer->userinfo.GetPlayerClassNum();
						str = GetPrintableDisplayName(statusBar->CPlayer->cls);
						RealignString();
					}
					break;
				case AMMO1TAG:
					SetStringToTag(statusBar->ammo1);
					break;
				case AMMO2TAG:
					SetStringToTag(statusBar->ammo2);
					break;
				case WEAPONTAG:
					SetStringToTag(statusBar->CPlayer->ReadyWeapon);
					break;
				case INVENTORYTAG:
					SetStringToTag(statusBar->CPlayer->mo->InvSel);
					break;
				case PLAYERNAME:
					// Can't think of a good way to detect changes to this, so
					// I guess copying it every tick will have to do.
					str = statusBar->CPlayer->userinfo.GetName();
					RealignString();
					break;
				case GLOBALVAR:
					if(ACS_GlobalVars[valueArgument] != cache)
					{
						cache = ACS_GlobalVars[valueArgument];
						str = FBehavior::StaticLookupString(ACS_GlobalVars[valueArgument]);
						RealignString();
					}
					break;
				case GLOBALARRAY:
					if(ACS_GlobalArrays[valueArgument][consoleplayer] != cache)
					{
						cache = ACS_GlobalArrays[valueArgument][consoleplayer];
						str = FBehavior::StaticLookupString(ACS_GlobalArrays[valueArgument][consoleplayer]);
						RealignString();
					}
					break;
				case TIME:
					str.Format("%02d:%02d:%02d", (level.time/TICRATE)/3600, ((level.time/TICRATE)%3600)/60, (level.time/TICRATE)%60);
					break;
				case LOGTEXT:
					str = statusBar->CPlayer->LogText;
					break;
				default:
					break;
			}
		}
	protected:
		enum StringAlignment
		{
			ALIGN_RIGHT,
			ALIGN_LEFT,
			ALIGN_CENTER,
		};

		void RealignString()
		{
			x = startX;
			switch (alignment)
			{
			case ALIGN_LEFT:
				break;
			case ALIGN_RIGHT:
				if(script->spacingCharacter == '\0')
					x -= static_cast<int> (font->StringWidth(str)+(spacing * str.Len()));
				else //monospaced, so just multiplay the character size
					x -= static_cast<int> ((font->GetCharWidth((unsigned char) script->spacingCharacter) + spacing) * str.Len());
				break;
			case ALIGN_CENTER:
				if(script->spacingCharacter == '\0')
					x -= static_cast<int> ((font->StringWidth(str)+(spacing * str.Len())) / 2);
				else
					x -= static_cast<int> ((font->GetCharWidth((unsigned char) script->spacingCharacter) + spacing) * str.Len() / 2);
				break;
			}
		}

		enum StringValueType
		{
			LEVELNAME,
			LEVELLUMP,
			SKILLNAME,
			PLAYERCLASS,
			PLAYERNAME,
			AMMO1TAG,
			AMMO2TAG,
			WEAPONTAG,
			INVENTORYTAG,
			GLOBALVAR,
			GLOBALARRAY,
			TIME,
			LOGTEXT,

			CONSTANT
		};

		bool				lineBreaks;
		int					breakWidth;
		bool				shadow;
		int					shadowX;
		int					shadowY;
		int					spacing;
		FFont				*font;
		EColorRange			translation;
		SBarInfoCoordinate	startX;
		SBarInfoCoordinate	x;
		SBarInfoCoordinate	y;
		int					cache; /// General purpose cache.
		StringValueType		strValue;
		int					valueArgument;
		FString				str;
		StringAlignment		alignment;

	private:
		void SetStringToTag(AActor *actor)
		{
			if(actor != NULL)
			{
				if(actor->GetClass()->ClassIndex != cache)
				{
					cache = actor->GetClass()->ClassIndex;
					str = actor->GetTag();
					RealignString();
				}
			}
			else
			{
				cache = -1;
				str = "";
			}
		}
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawNumber : public CommandDrawString
{
	public:
		CommandDrawNumber(SBarInfo *script) : CommandDrawString(script),
			fillZeros(false), whenNotZero(false), dontCap(false),
			usePrefix(false), interpolationSpeed(0), drawValue(0), length(3),
			lowValue(-1), lowTranslation(CR_UNTRANSLATED), highValue(-1),
			highTranslation(CR_UNTRANSLATED), value(CONSTANT),
			inventoryItem(NULL)
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_IntConst);
			length = sc.Number;
			sc.MustGetToken(',');
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			font = V_GetFont(sc.String);
			if(font == NULL)
			{
				sc.ScriptMessage("Unknown font '%s'.", sc.String);
				font = SmallFont;
			}
			sc.MustGetToken(',');
			normalTranslation = GetTranslation(sc);
			sc.MustGetToken(',');
			if(sc.CheckToken(TK_IntConst))
			{
				value = CONSTANT;
				valueArgument = sc.Number;
				sc.MustGetToken(',');
			}
			else
			{
				if(!sc.CheckToken(TK_StringConst))
					sc.MustGetToken(TK_Identifier);
				value = INVENTORY;
				if(sc.TokenType == TK_Identifier)
				{
					if(sc.Compare("health"))
						value = HEALTH;
					else if(sc.Compare("armor"))
						value = ARMOR;
					else if(sc.Compare("ammo1"))
						value = AMMO1;
					else if(sc.Compare("ammo2"))
						value = AMMO2;
					else if(sc.Compare("ammo1capacity"))
						value = AMMO1CAPACITY;
					else if(sc.Compare("ammo2capacity"))
						value = AMMO2CAPACITY;
					else if(sc.Compare("score"))
						value = SCORE;
					else if(sc.Compare("ammo")) //request the next string to be an ammo type
					{
						bool parenthesized = sc.CheckToken('(');

						value = AMMO;
						if(!parenthesized || !sc.CheckToken(TK_StringConst))
							sc.MustGetToken(TK_Identifier);
						inventoryItem = PClass::FindClass(sc.String);
						if(inventoryItem == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(inventoryItem)) //must be a kind of ammo
						{
							sc.ScriptMessage("'%s' is not a type of ammo.", sc.String);
							inventoryItem = RUNTIME_CLASS(AAmmo);
						}

						if(parenthesized) sc.MustGetToken(')');
					}
					else if(sc.Compare("ammocapacity"))
					{
						bool parenthesized = sc.CheckToken('(');

						value = AMMOCAPACITY;
						if(!parenthesized || !sc.CheckToken(TK_StringConst))
							sc.MustGetToken(TK_Identifier);
						inventoryItem = PClass::FindClass(sc.String);
						if(inventoryItem == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(inventoryItem)) //must be a kind of ammo
						{
							sc.ScriptMessage("'%s' is not a type of ammo.", sc.String);
							inventoryItem = RUNTIME_CLASS(AAmmo);
						}

						if(parenthesized) sc.MustGetToken(')');
					}
					else if(sc.Compare("frags"))
						value = FRAGS;
					else if(sc.Compare("kills"))
						value = KILLS;
					else if(sc.Compare("monsters"))
						value = MONSTERS;
					else if(sc.Compare("items"))
						value = ITEMS;
					else if(sc.Compare("totalitems"))
						value = TOTALITEMS;
					else if(sc.Compare("secrets"))
						value = SECRETS;
					else if(sc.Compare("totalsecrets"))
						value = TOTALSECRETS;
					else if(sc.Compare("armorclass"))
						value = ARMORCLASS;
					else if(sc.Compare("savepercent"))
						value = SAVEPERCENT;
					else if(sc.Compare("airtime"))
						value = AIRTIME;
					else if(sc.Compare("accuracy"))
						value = ACCURACY;
					else if(sc.Compare("stamina"))
						value = STAMINA;
					else if(sc.Compare("keys"))
						value = KEYS;
					else if(sc.Compare("globalvar"))
					{
						bool parenthesized = sc.CheckToken('(');

						value = GLOBALVAR;
						sc.MustGetToken(TK_IntConst);
						if(sc.Number < 0 || sc.Number >= NUM_GLOBALVARS)
							sc.ScriptError("Global variable number out of range: %d", sc.Number);
						valueArgument = sc.Number;

						if(parenthesized) sc.MustGetToken(')');
					}
					else if(sc.Compare("globalarray")) //acts like variable[playernumber()]
					{
						bool parenthesized = sc.CheckToken('(');

						value = GLOBALARRAY;
						sc.MustGetToken(TK_IntConst);
						if(sc.Number < 0 || sc.Number >= NUM_GLOBALVARS)
							sc.ScriptError("Global variable number out of range: %d", sc.Number);
						valueArgument = sc.Number;

						if(parenthesized) sc.MustGetToken(')');
					}
					else if(sc.Compare("poweruptime"))
					{
						bool parenthesized = sc.CheckToken('(');

						value = POWERUPTIME;
						if(!parenthesized || !sc.CheckToken(TK_StringConst))
							sc.MustGetToken(TK_Identifier);
						inventoryItem = PClass::FindClass(sc.String);
						if(inventoryItem == NULL || !RUNTIME_CLASS(APowerupGiver)->IsAncestorOf(inventoryItem))
						{
							sc.ScriptMessage("'%s' is not a type of PowerupGiver.", sc.String);
							inventoryItem = RUNTIME_CLASS(APowerupGiver);
						}

						if(parenthesized) sc.MustGetToken(')');
					}
				}
				if(value == INVENTORY)
				{
					inventoryItem = PClass::FindClass(sc.String);
					if(inventoryItem == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(inventoryItem)) //must be a kind of ammo
					{
						sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
						inventoryItem = RUNTIME_CLASS(AInventory);
					}
				}
				sc.MustGetToken(',');
			}
			while(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("fillzeros"))
					fillZeros = true;
				else if(sc.Compare("whennotzero"))
					whenNotZero = true;
				else if(sc.Compare("dontcap"))
					dontCap = true;
				else if(sc.Compare("drawshadow"))
				{
					if(sc.CheckToken('('))
					{
						sc.MustGetToken(TK_IntConst);
						shadowX = sc.Number;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						shadowY = sc.Number;
						sc.MustGetToken(')');
					}
					shadow = true;
				}
				else if(sc.Compare("interpolate"))
				{
					sc.MustGetToken('(');
					sc.MustGetToken(TK_IntConst);
					interpolationSpeed = sc.Number;
					sc.MustGetToken(')');
				}
				else if(sc.Compare("alignment"))
				{
					sc.MustGetToken('(');
					sc.MustGetToken(TK_Identifier);
					if(sc.Compare("right"))
						alignment = ALIGN_RIGHT;
					else if(sc.Compare("left"))
						alignment = ALIGN_LEFT;
					else if(sc.Compare("center"))
						alignment = ALIGN_CENTER;
					else
						sc.ScriptError("Unknown alignment '%s'.", sc.String);
					sc.MustGetToken(')');
				}
				else if(sc.Compare("prefix"))
				{
					usePrefix = true;
					sc.MustGetToken('(');
					ParseStringValue(sc);
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					prefixPadding = sc.String;
					if(strValue == CommandDrawString::CONSTANT)
					{
						usePrefix = false; // Use prefix just determines if we tick the string.
						prefixPadding = str + prefixPadding;
					}
					sc.MustGetToken(')');
				}
				else
					sc.ScriptError("Unknown flag '%s'.", sc.String);
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
			GetCoordinates(sc, fullScreenOffsets, startX, y);
			if(sc.CheckToken(','))
			{
				bool needsComma = false;
				if(sc.CheckToken(TK_IntConst)) //font spacing
				{
					spacing = sc.Number;
					needsComma = true;
				}
				if(!needsComma || sc.CheckToken(',')) //2nd coloring for "low-on" value
				{
					lowTranslation = GetTranslation(sc);
					sc.MustGetToken(',');
					sc.MustGetToken(TK_IntConst);
					lowValue = sc.Number;
					if(sc.CheckToken(',')) //3rd coloring for "high-on" value
					{
						highTranslation = GetTranslation(sc);
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						highValue = sc.Number;
					}
				}
			}
			sc.MustGetToken(';');

			if(value == HEALTH)
				interpolationSpeed = script->interpolateHealth ? script->interpolationSpeed : interpolationSpeed;
			else if(value == ARMOR)
				interpolationSpeed = script->interpolateArmor ? script->armorInterpolationSpeed : interpolationSpeed;
		}
		void	Reset()
		{
			drawValue = 0;
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			if(usePrefix)
			{
				cache = -1; // Disable the cache since we are using the same variables.
				CommandDrawString::Tick(block, statusBar, hudChanged);
			}

			int num = valueArgument;
			switch(value)
			{
				case HEALTH:
					num = statusBar->CPlayer->mo->health;
					if(script->lowerHealthCap && num < 0) //health shouldn't display negatives
						num = 0;
					if(script->interpolateHealth)
						interpolationSpeed = script->interpolationSpeed;
					break;
				case ARMOR:
					num = statusBar->armor != NULL ? statusBar->armor->Amount : 0;
					if(script->interpolateArmor)
						interpolationSpeed = script->armorInterpolationSpeed;
					break;
				case AMMO1:
					if(statusBar->ammo1 == NULL) //no ammo, do not draw
					{
						str = "";
						return;
					}
					num = statusBar->ammocount1;
					break;
				case AMMO2:
					if(statusBar->ammo2 == NULL) //no ammo, do not draw
					{
						str = "";
						return;
					}
					num = statusBar->ammocount2;
					break;
				case AMMO:
				{
					AInventory* item = statusBar->CPlayer->mo->FindInventory(inventoryItem);
					if(item != NULL)
						num = item->Amount;
					else
						num = 0;
					break;
				}
				case AMMO1CAPACITY:
					if(statusBar->ammo1 == NULL) //no ammo, do not draw
					{
						str = "";
						return;
					}
					else
						num = statusBar->ammo1->MaxAmount;
					break;
				case AMMO2CAPACITY:
					if(statusBar->ammo2 == NULL) //no ammo, do not draw
					{
						str = "";
						return;
					}
					else
						num = statusBar->ammo2->MaxAmount;
					break;
				case AMMOCAPACITY:
				{
					AInventory* item = statusBar->CPlayer->mo->FindInventory(inventoryItem);
					if(item != NULL)
						num = item->MaxAmount;
					else
						num = ((AInventory *)GetDefaultByType(inventoryItem))->MaxAmount;
					break;
				}
				case FRAGS:
					num = statusBar->CPlayer->fragcount;
					break;
				case KILLS:
					num = level.killed_monsters;
					break;
				case MONSTERS:
					num = level.total_monsters;
					break;
				case ITEMS:
					num = level.found_items;
					break;
				case TOTALITEMS:
					num = level.total_items;
					break;
				case SECRETS:
					num = level.found_secrets;
					break;
				case SCORE:
					num = statusBar->CPlayer->mo->Score;
					break;
				case TOTALSECRETS:
					num = level.total_secrets;
					break;
				case ARMORCLASS:
				case SAVEPERCENT:
				{
					AHexenArmor *harmor = statusBar->CPlayer->mo->FindInventory<AHexenArmor>();
					if(harmor != NULL)
					{
						num = harmor->Slots[0] + harmor->Slots[1] +
							harmor->Slots[2] + harmor->Slots[3] + harmor->Slots[4];
					}
					//Hexen counts basic armor also so we should too.
					if(statusBar->armor != NULL)
					{
						num += FixedMul(statusBar->armor->SavePercent, 100*FRACUNIT);
					}
					if(value == ARMORCLASS)
						num /= (5*FRACUNIT);
					else
						num >>= FRACBITS;
					break;
				}
				case GLOBALVAR:
					num = ACS_GlobalVars[valueArgument];
					break;
				case GLOBALARRAY:
					num = ACS_GlobalArrays[valueArgument][consoleplayer];
					break;
				case POWERUPTIME:
				{
					//Get the PowerupType and check to see if the player has any in inventory.
					const PClass* powerupType = ((APowerupGiver*) GetDefaultByType(inventoryItem))->PowerupType;
					APowerup* powerup = (APowerup*) statusBar->CPlayer->mo->FindInventory(powerupType);
					if(powerup != NULL)
						num = powerup->EffectTics / TICRATE + 1;
					break;
				}
				case INVENTORY:
				{
					AInventory* item = statusBar->CPlayer->mo->FindInventory(inventoryItem);
					if(item != NULL)
						num = item->Amount;
					else
						num = 0;
					break;
				}
				case AIRTIME:
				{
					if(statusBar->CPlayer->mo->waterlevel < 3)
						num = level.airsupply/TICRATE;
					else
						num = clamp<int>((statusBar->CPlayer->air_finished - level.time + (TICRATE-1))/TICRATE, 0, INT_MAX);
					break;
				}
				case SELECTEDINVENTORY:
					if(statusBar->CPlayer->mo->InvSel != NULL)
						num = statusBar->CPlayer->mo->InvSel->Amount;
					break;
				case ACCURACY:
					num = statusBar->CPlayer->mo->accuracy;
					break;
				case STAMINA:
					num = statusBar->CPlayer->mo->stamina;
					break;
				case KEYS:
					num = 0;
					for(AInventory *item = statusBar->CPlayer->mo->Inventory;item != NULL;item = item->Inventory)
					{
						if(item->IsKindOf(RUNTIME_CLASS(AKey)))
							num++;
					}
					break;
				default: break;
			}
			if(interpolationSpeed != 0 && (!hudChanged || level.time == 1))
			{
				if(num < drawValue)
					drawValue -= clamp<int>((drawValue - num) >> 2, 1, interpolationSpeed);
				else if(drawValue < num)
					drawValue += clamp<int>((num - drawValue) >> 2, 1, interpolationSpeed);
			}
			else
				drawValue = num;
			if(whenNotZero && drawValue == 0)
			{
				str = "";
				return;
			}
		
			translation = normalTranslation;
			if(lowValue != -1 && drawValue <= lowValue) //low
				translation = lowTranslation;
			else if(highValue != -1 && drawValue >= highValue) //high
				translation = highTranslation;

			bool useFillZeros = fillZeros;
			if(!dontCap)
			{
				// 10^9 is a largest we can hold in a 32-bit int.  So if we go any larger we have to toss out the positions limit.
				int maxval = length <= 9 ? (int) ceil(pow(10., length))-1 : INT_MAX;
				if(!fillZeros || length == 1)
					drawValue = clamp(drawValue, -maxval, maxval);
				else //The community wanted negatives to take the last digit, but we can only do this if there is room
					drawValue = clamp(drawValue, length <= 9 ? (int) -(ceil(pow(10., length-1))-1) : INT_MIN, maxval);
			}
			else if(length <= 9)
			{
				int limit = (int) ceil(pow(10., length > 1 && drawValue < 0 ? length - 1 : length));
				if(drawValue >= limit)
					useFillZeros = true;
				drawValue = drawValue%limit;
			}

			if(useFillZeros)
				str.Format("%s%s%0*d", usePrefix ? str.GetChars() : "", prefixPadding.GetChars(), drawValue < 0 ? length - 1 : length, drawValue);
			else
				str.Format("%s%s%d", usePrefix ? str.GetChars() : "", prefixPadding.GetChars(), drawValue);

			RealignString();
		}
	protected:
		enum ValueType
		{
			HEALTH,
			ARMOR,
			AMMO1,
			AMMO2,
			AMMO,
			AMMO1CAPACITY,
			AMMO2CAPACITY,
			AMMOCAPACITY,
			FRAGS,
			INVENTORY,
			KILLS,
			MONSTERS,
			ITEMS,
			TOTALITEMS,
			SECRETS,
			TOTALSECRETS,
			ARMORCLASS,
			GLOBALVAR,
			GLOBALARRAY,
			POWERUPTIME,
			AIRTIME,
			SELECTEDINVENTORY,
			SCORE,
			SAVEPERCENT,
			ACCURACY,
			STAMINA,
			KEYS,

			CONSTANT
		};

		bool				fillZeros;
		bool				whenNotZero;
		bool				dontCap;
		bool				usePrefix;

		int					interpolationSpeed;
		int					drawValue;

		int					length;
		int					lowValue;
		EColorRange			lowTranslation;
		int					highValue;
		EColorRange			highTranslation;
		EColorRange			normalTranslation;
		ValueType			value;
		const PClass		*inventoryItem;

		FString				prefixPadding;

		friend class CommandDrawInventoryBar;
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawMugShot : public SBarInfoCommand
{
	public:
		CommandDrawMugShot(SBarInfo *script) : SBarInfoCommand(script),
			accuracy(5), stateFlags(FMugShot::STANDARD)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			FTexture *face = script->MugShot.GetFace(statusBar->CPlayer, defaultFace, accuracy, stateFlags);
			if (face != NULL)
				statusBar->DrawGraphic(face, x, y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(sc.CheckToken(TK_StringConst))
			{
				defaultFace = sc.String;
				if(defaultFace.Len() > 3)
					sc.ScriptError("Default can not be longer than 3 characters.");
				sc.MustGetToken(',');
			}
			sc.MustGetToken(TK_IntConst); //accuracy
			if(sc.Number < 1 || sc.Number > 9)
				sc.ScriptError("Expected a number between 1 and 9, got %d instead.", sc.Number);
			accuracy = sc.Number;
			sc.MustGetToken(',');
			while(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("xdeathface"))
					stateFlags = static_cast<FMugShot::StateFlags> (stateFlags|FMugShot::XDEATHFACE);
				else if(sc.Compare("animatedgodmode"))
					stateFlags = static_cast<FMugShot::StateFlags> (stateFlags|FMugShot::ANIMATEDGODMODE);
				else if(sc.Compare("disablegrin"))
					stateFlags = static_cast<FMugShot::StateFlags> (stateFlags|FMugShot::DISABLEGRIN);
				else if(sc.Compare("disableouch"))
					stateFlags = static_cast<FMugShot::StateFlags> (stateFlags|FMugShot::DISABLEOUCH);
				else if(sc.Compare("disablepain"))
					stateFlags = static_cast<FMugShot::StateFlags> (stateFlags|FMugShot::DISABLEPAIN);
				else if(sc.Compare("disablerampage"))
					stateFlags = static_cast<FMugShot::StateFlags> (stateFlags|FMugShot::DISABLERAMPAGE);
				else
					sc.ScriptError("Unknown flag '%s'.", sc.String);
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
		
			GetCoordinates(sc, fullScreenOffsets, x, y);
			sc.MustGetToken(';');
		}
		void Reset()
		{
			script->MugShot.Reset();
		}

	protected:
		FString					defaultFace; //Deprecated

		int						accuracy;
		FMugShot::StateFlags	stateFlags;
		SBarInfoCoordinate		x;
		SBarInfoCoordinate		y;
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawSelectedInventory : public CommandDrawImage, private CommandDrawNumber
{
	public:
		CommandDrawSelectedInventory(SBarInfo *script) : CommandDrawImage(script),
			CommandDrawNumber(script), alternateOnEmpty(false),
			artiflash(false), alwaysShowCounter(false)
		{
			length = INT_MAX; // Counter size
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			if(alternateOnEmpty)
				SBarInfoCommandFlowControl::Draw(block, statusBar);

			if(statusBar->CPlayer->mo->InvSel != NULL && !(level.flags & LEVEL_NOINVENTORYBAR))
			{
				if(artiflash && artiflashTick)
				{
					statusBar->DrawGraphic(statusBar->Images[ARTIFLASH_OFFSET+(4-artiflashTick)], imgx, imgy, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(),
						translatable, false, offset);
				}
				else
				{
					if(itemflash && itemflashFade)
					{
						fixed_t flashAlpha = fixed_t(((double) block->Alpha() / (double) FRACUNIT) * ((double) itemflashFade / (double) OPAQUE) * FRACUNIT);
						statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgCURSOR], imgx-4, imgy+2, block->XOffset(), block->YOffset(), flashAlpha, block->FullScreenOffsets(),
							translatable, false, offset);
					}
					CommandDrawImage::Draw(block, statusBar);
				}
				if(alwaysShowCounter || statusBar->CPlayer->mo->InvSel->Amount != 1)
					CommandDrawNumber::Draw(block, statusBar);
			}
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			type = SELECTEDINVENTORYICON;
			value = SELECTEDINVENTORY;
			while(true) //go until we get a font (non-flag)
			{
				if(!sc.CheckToken(TK_StringConst))
					sc.MustGetToken(TK_Identifier);
				bool isFont = sc.TokenType != TK_Identifier;
				if(sc.TokenType == TK_Identifier)
				{
					if(sc.Compare("alternateonempty"))
						alternateOnEmpty = true;
					else if(sc.Compare("artiflash"))
						artiflash = true;
					else if(sc.Compare("alwaysshowcounter"))
						alwaysShowCounter = true;
					else if(sc.Compare("itemflash"))
						itemflash = true;
					else if(sc.Compare("center"))
						offset = CENTER;
					else if(sc.Compare("centerbottom"))
						offset = static_cast<Offset> (HMIDDLE|BOTTOM);
					else if(sc.Compare("drawshadow"))
					{
						if(sc.CheckToken('('))
						{
							sc.MustGetToken(TK_IntConst);
							shadowX = sc.Number;
							sc.MustGetToken(',');
							sc.MustGetToken(TK_IntConst);
							shadowY = sc.Number;
							sc.MustGetToken(')');
						}
						shadow = true;
					}
					else
						isFont = true;
				}
				if(isFont)
				{
					font = V_GetFont(sc.String);
					if(font == NULL)
					{
						sc.ScriptMessage("Unknown font '%s'.", sc.String);
						font = SmallFont;
					}
					sc.MustGetToken(',');
					break;
				}
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
			CommandDrawImage::GetCoordinates(sc, fullScreenOffsets, imgx, imgy);
			startX = imgx + 30;
			y = imgy + 24;
			normalTranslation = CR_GOLD;
			if(sc.CheckToken(',')) //more font information
			{
				CommandDrawNumber::GetCoordinates(sc, fullScreenOffsets, startX, y);
				if(sc.CheckToken(','))
				{
					normalTranslation = CommandDrawNumber::GetTranslation(sc);
					if(sc.CheckToken(','))
					{
						sc.MustGetToken(TK_IntConst);
						spacing = sc.Number;
					}
				}
			}
			if(alternateOnEmpty)
				SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
			else
				sc.MustGetToken(';');
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			if(artiflashTick > 0)
				artiflashTick--;
			if(itemflashFade > 0)
			{
				itemflashFade -= FRACUNIT/14;
				if(itemflashFade < 0)
					itemflashFade = 0;
			}

			SetTruth(statusBar->CPlayer->mo->InvSel == NULL || (level.flags & LEVEL_NOINVENTORYBAR), block, statusBar);

			CommandDrawImage::Tick(block, statusBar, hudChanged);
			CommandDrawNumber::Tick(block, statusBar, hudChanged);
		}

		static void	Flash() { artiflashTick = 4; itemflashFade = FRACUNIT*3/4; }
	protected:
		bool	alternateOnEmpty;
		bool	artiflash;
		bool	alwaysShowCounter;
		bool	itemflash;

		static int		artiflashTick;
		static fixed_t	itemflashFade;
};
int CommandDrawSelectedInventory::artiflashTick = 4;
int CommandDrawSelectedInventory::itemflashFade = FRACUNIT*3/4;

void DSBarInfo::FlashItem(const PClass *itemtype)
{
	CommandDrawSelectedInventory::Flash();
}

////////////////////////////////////////////////////////////////////////////////

class CommandGameMode : public SBarInfoCommandFlowControl
{
	public:
		CommandGameMode(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			modes(static_cast<GameModes> (0))
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			do
			{
				sc.MustGetToken(TK_Identifier);
				modes |= static_cast<GameModes> (1<<sc.MustMatchString(modeNames));
			}
			while(sc.CheckToken(','));
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			SetTruth((!multiplayer && (modes & SINGLEPLAYER)) ||
				(deathmatch && (modes & DEATHMATCH)) ||
				(multiplayer && !deathmatch && (modes & COOPERATIVE)) ||
				(teamplay && (modes & TEAMGAME)), block, statusBar);
		}
	protected:
		static const char* const	modeNames[];
		enum GameModes
		{
			SINGLEPLAYER	= 0x1,
			COOPERATIVE		= 0x2,
			DEATHMATCH		= 0x4,
			TEAMGAME		= 0x8
		};

		unsigned int	modes;
};

const char* const CommandGameMode::modeNames[] =
{
	"singleplayer",
	"cooperative",
	"deathmatch",
	"teamgame",
	NULL
};

////////////////////////////////////////////////////////////////////////////////

class CommandUsesAmmo : public SBarInfoCommandFlowControl
{
	public:
		CommandUsesAmmo(SBarInfo *script)  : SBarInfoCommandFlowControl(script),
			negate(false)
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("not"))
					negate = true;
				else
					sc.ScriptError("Expected 'not', but got '%s' instead.", sc.String);
			}
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			SetTruth((statusBar->CPlayer->ReadyWeapon != NULL && (statusBar->CPlayer->ReadyWeapon->AmmoType1 != NULL || statusBar->CPlayer->ReadyWeapon->AmmoType2 != NULL)) ^ negate, block, statusBar);
		}
	protected:
		bool	negate;
};

////////////////////////////////////////////////////////////////////////////////

class CommandUsesSecondaryAmmo : public CommandUsesAmmo
{
	public:
		CommandUsesSecondaryAmmo(SBarInfo *script) : CommandUsesAmmo(script)
		{
		}

		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			SetTruth((statusBar->CPlayer->ReadyWeapon != NULL && statusBar->CPlayer->ReadyWeapon->AmmoType2 != NULL && statusBar->CPlayer->ReadyWeapon->AmmoType1 != statusBar->CPlayer->ReadyWeapon->AmmoType2) ^ negate, block, statusBar);
		}
};

////////////////////////////////////////////////////////////////////////////////

class CommandInventoryBarNotVisible : public SBarInfoCommandFlowControl
{
	public:
		CommandInventoryBarNotVisible(SBarInfo *script) : SBarInfoCommandFlowControl(script)
		{
		}

		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			SetTruth(statusBar->CPlayer->inventorytics <= 0 || (level.flags & LEVEL_NOINVENTORYBAR), block, statusBar);
		}
};

////////////////////////////////////////////////////////////////////////////////

class CommandAspectRatio : public SBarInfoCommandFlowControl
{
	public:
		CommandAspectRatio(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			ratio(ASPECTRATIO_4_3)
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_StringConst);
			if(sc.Compare("4:3"))
				ratio = ASPECTRATIO_4_3;
			else if(sc.Compare("16:9"))
				ratio = ASPECTRATIO_16_9;
			else if(sc.Compare("16:10"))
				ratio = ASPECTRATIO_16_10;
			else if(sc.Compare("17:10"))
				ratio = ASPECTRATIO_17_10;
			else if(sc.Compare("5:4"))
				ratio = ASPECTRATIO_5_4;
			else
				sc.ScriptError("Unkown aspect ratio: %s", sc.String);

			// Make this ratio known and map to itself
			// In the future, should another aspect ratio get added, you'd want
			// to also make any wider ratio remap to this one if suitable.
			ratioMap[ratio] = ratio;

			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			SetTruth(ratioMap[CheckRatio(screen->GetWidth(), screen->GetHeight())] == ratio, block, statusBar);
		}
	protected:
		enum Ratio
		{
			ASPECTRATIO_4_3 = 0,
			ASPECTRATIO_16_9 = 1,
			ASPECTRATIO_16_10 = 2,
			ASPECTRATIO_17_10 = 3,
			ASPECTRATIO_5_4 = 4
		};
		// Since the number of aspect ratios may change at any time, we should
		// track what aspect ratios the statusbar supports and use the widest
		// or tallest available ratio.
		static Ratio	ratioMap[5];

		Ratio			ratio;
};
CommandAspectRatio::Ratio CommandAspectRatio::ratioMap[5] = {ASPECTRATIO_4_3,ASPECTRATIO_16_9,ASPECTRATIO_16_10,ASPECTRATIO_16_10,ASPECTRATIO_5_4};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawShader : public SBarInfoCommand
{
	public:
		CommandDrawShader(SBarInfo *script) : SBarInfoCommand(script),
			vertical(false), reverse(false), width(1), height(1)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			statusBar->DrawGraphic(&shaders[(vertical<<1) + reverse], x, y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, false, 0, true, width, height);
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_IntConst);
			width = sc.Number;
			if(sc.Number < 1)
				sc.ScriptError("Width must be greater than 1.");
			sc.MustGetToken(',');
			sc.MustGetToken(TK_IntConst);
			height = sc.Number;
			if(sc.Number < 1)
				sc.ScriptError("Height must be greater than 1.");
			sc.MustGetToken(',');
			sc.MustGetToken(TK_Identifier);
			if(sc.Compare("vertical"))
				vertical = true;
			else if(!sc.Compare("horizontal"))
				sc.ScriptError("Unknown direction '%s'.", sc.String);
			sc.MustGetToken(',');
			if(sc.CheckToken(TK_Identifier))
			{
				if(!sc.Compare("reverse"))
				{
					sc.ScriptError("Exspected 'reverse', got '%s' instead.", sc.String);
				}
				reverse = true;
				sc.MustGetToken(',');
			}
			GetCoordinates(sc, fullScreenOffsets, x, y);
			sc.MustGetToken(';');
		}
	protected:
		bool				vertical;
		bool				reverse;
		unsigned int		width;
		unsigned int		height;
		SBarInfoCoordinate	x;
		SBarInfoCoordinate	y;
	private:
		class FBarShader : public FTexture
		{
		public:
			FBarShader(bool vertical, bool reverse)
			{
				int i;

				Width = vertical ? 2 : 256;
				Height = vertical ? 256 : 2;
				CalcBitSize();

				// Fill the column/row with shading values.
				// Vertical shaders have have minimum alpha at the top
				// and maximum alpha at the bottom, unless flipped by
				// setting reverse to true. Horizontal shaders are just
				// the opposite.
				if (vertical)
				{
					if (!reverse)
					{
						for (i = 0; i < 256; ++i)
						{
							Pixels[i] = i;
							Pixels[256+i] = i;
						}
					}
					else
					{
						for (i = 0; i < 256; ++i)
						{
							Pixels[i] = 255 - i;
							Pixels[256+i] = 255 -i;
						}
					}
				}
				else
				{
					if (!reverse)
					{
						for (i = 0; i < 256; ++i)
						{
							Pixels[i*2] = 255 - i;
							Pixels[i*2+1] = 255 - i;
						}
					}
					else
					{
						for (i = 0; i < 256; ++i)
						{
							Pixels[i*2] = i;
							Pixels[i*2+1] = i;
						}
					}
				}
				DummySpan[0].TopOffset = 0;
				DummySpan[0].Length = vertical ? 256 : 2;
				DummySpan[1].TopOffset = 0;
				DummySpan[1].Length = 0;
			}
			const BYTE *GetColumn(unsigned int column, const Span **spans_out)
			{
				if (spans_out != NULL)
				{
					*spans_out = DummySpan;
				}
				return Pixels + ((column & WidthMask) << HeightBits);
			}
			const BYTE *GetPixels() { return Pixels; }
			void Unload() {}
		private:
			BYTE Pixels[512];
			Span DummySpan[2];
		};

		static FBarShader	shaders[4];
};

CommandDrawShader::FBarShader CommandDrawShader::shaders[4] =
{
	FBarShader(false, false), FBarShader(false, true),
	FBarShader(true, false), FBarShader(true, true)
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawInventoryBar : public SBarInfoCommand
{
	public:
		CommandDrawInventoryBar(SBarInfo *script) : SBarInfoCommand(script),
			style(STYLE_Doom), size(7), alwaysShow(false), noArtibox(false),
			noArrows(false), alwaysShowCounter(false), translucent(false),
			vertical(false), shadow(false), shadowX(2), shadowY(2),
			counters(NULL), font(NULL), translation(CR_GOLD),
			fontSpacing(0)
		{
		}
		~CommandDrawInventoryBar()
		{
			if(counters != NULL)
			{
				for(unsigned int i = 0;i < size;i++)
					delete counters[i];
				delete[] counters;
			}
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			int spacing = GetCounterSpacing(statusBar);
		
			int bgalpha = block->Alpha();
			if(translucent)
				bgalpha = fixed_t((((double) block->Alpha() / (double) FRACUNIT) * ((double) HX_SHADOW / (double) FRACUNIT)) * FRACUNIT);
		
			AInventory *item;
			unsigned int i = 0;
			// If the player has no artifacts, don't draw the bar
			statusBar->CPlayer->mo->InvFirst = statusBar->ValidateInvFirst(size);
			if(statusBar->CPlayer->mo->InvFirst != NULL || alwaysShow)
			{
				for(item = statusBar->CPlayer->mo->InvFirst, i = 0; item != NULL && i < size; item = item->NextInv(), ++i)
				{
					SBarInfoCoordinate rx = x + (!vertical ? i*spacing : 0);
					SBarInfoCoordinate ry = y + (vertical ? i*spacing : 0);
					if(!noArtibox)
						statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgARTIBOX], rx, ry, block->XOffset(), block->YOffset(), bgalpha, block->FullScreenOffsets());
		
					if(style != STYLE_Strife) //Strife draws the cursor before the icons
						statusBar->DrawGraphic(TexMan(item->Icon), rx - (style == STYLE_HexenStrict ? 2 : 0), ry - (style == STYLE_HexenStrict ? 1 : 0), block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, item->Amount <= 0);
					if(item == statusBar->CPlayer->mo->InvSel)
					{
						if(style == STYLE_Heretic)
							statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgSELECTBOX], rx, ry+29, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
						else if(style == STYLE_Hexen)
							statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgSELECTBOX], rx, ry-1, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
						else if(style == STYLE_HexenStrict)
							statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgSELECTBOX], rx-1, ry-1, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
						else if(style == STYLE_Strife)
							statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgCURSOR], rx-6, ry-2, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
						else
							statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgSELECTBOX], rx, ry, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
					}
					if(style == STYLE_Strife)
						statusBar->DrawGraphic(TexMan(item->Icon), rx, ry, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, item->Amount <= 0);
					if(counters != NULL && (alwaysShowCounter || item->Amount != 1))
					{
						counters[i]->valueArgument = item->Amount;
						counters[i]->Draw(block, statusBar);
					}
				}
				for (; i < size && !noArtibox; ++i)
					statusBar->DrawGraphic(statusBar->Images[statusBar->invBarOffset + imgARTIBOX], x + (!vertical ? (i*spacing) : 0), y + (vertical ? (i*spacing) : 0), block->XOffset(), block->YOffset(), bgalpha, block->FullScreenOffsets());
		
				// Is there something to the left?
				if (!noArrows && statusBar->CPlayer->mo->FirstInv() != statusBar->CPlayer->mo->InvFirst)
				{
					int offset = (style != STYLE_Strife ? (style != STYLE_HexenStrict ? -12 : -10) : 14);
					int yOffset = style != STYLE_HexenStrict ? 0 : -1;
					statusBar->DrawGraphic(statusBar->Images[!(gametic & 4) ?
						statusBar->invBarOffset + imgINVLFGEM1 : statusBar->invBarOffset + imgINVLFGEM2], x + (!vertical ? offset : yOffset), y + (vertical ? offset : yOffset), block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
				}
				// Is there something to the right?
				if (!noArrows && item != NULL)
				{
					int offset = (style != STYLE_Strife ? (style != STYLE_HexenStrict ? size*31+2 : size*31) : size*35-4);
					int yOffset = style != STYLE_HexenStrict ? 0 : -1;
					statusBar->DrawGraphic(statusBar->Images[!(gametic & 4) ?
						statusBar->invBarOffset + imgINVRTGEM1 : statusBar->invBarOffset + imgINVRTGEM2], x + (!vertical ? offset : yOffset), y + (vertical ? offset : yOffset), block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
				}
			}
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_Identifier);
			if(sc.Compare("Doom"))
				style = STYLE_Doom;
			else if(sc.Compare("Heretic"))
				style = STYLE_Heretic;
			else if(sc.Compare("Hexen"))
				style = STYLE_Hexen;
			else if(sc.Compare("HexenStrict"))
				style = STYLE_HexenStrict;
			else if(sc.Compare("Strife"))
				style = STYLE_Strife;
			else
				sc.ScriptError("Unknown style '%s'.", sc.String);
		
			sc.MustGetToken(',');
			while(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("alwaysshow"))
					alwaysShow = true;
				else if(sc.Compare("drawshadow"))
				{
					if(sc.CheckToken('('))
					{
						sc.MustGetToken(TK_IntConst);
						shadowX = sc.Number;
						sc.MustGetToken(',');
						sc.MustGetToken(TK_IntConst);
						shadowY = sc.Number;
						sc.MustGetToken(')');
					}
					shadow = true;
				}
				else if(sc.Compare("noartibox"))
					noArtibox = true;
				else if(sc.Compare("noarrows"))
					noArrows = true;
				else if(sc.Compare("alwaysshowcounter"))
					alwaysShowCounter = true;
				else if(sc.Compare("translucent"))
					translucent = true;
				else if(sc.Compare("vertical"))
					vertical = true;
				else
					sc.ScriptError("Unknown flag '%s'.", sc.String);
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
			sc.MustGetToken(TK_IntConst);
			size = sc.Number;
			sc.MustGetToken(',');
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			font = V_GetFont(sc.String);
			if(font == NULL)
			{
				sc.ScriptError("Unknown font '%s'.", sc.String);
				font = SmallFont;
			}
		
			sc.MustGetToken(',');
			GetCoordinates(sc, fullScreenOffsets, x, y);
			counterX = x + 26;
			counterY = y + 22;
			if(sc.CheckToken(',')) //more font information
			{
				GetCoordinates(sc, fullScreenOffsets, counterX, counterY);
				if(sc.CheckToken(','))
				{
					translation = GetTranslation(sc);
					if(sc.CheckToken(','))
					{
						sc.MustGetToken(TK_IntConst);
						fontSpacing = sc.Number;
					}
				}
			}
			sc.MustGetToken(';');
		}
		int GetCounterSpacing(const DSBarInfo *statusBar) const
		{
			FTexture *box = (style != STYLE_Strife)
				? statusBar->Images[statusBar->invBarOffset + imgARTIBOX]
				: statusBar->Images[statusBar->invBarOffset + imgCURSOR];
			if (box == NULL)
			{ // Don't crash without a graphic.
				return 32;
			}
			else
			{
				int spacing;
				if (!vertical)
				{
					spacing = box->GetScaledWidth();
				}
				else
				{
					spacing = box->GetScaledHeight();
				}
				return spacing + ((style != STYLE_Strife) ? 1 : -1);
			}
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			// Make the counters if need be.
			if(counters == NULL)
			{
				int spacing = GetCounterSpacing(statusBar);

				counters = new CommandDrawNumber*[size];
		
				for(unsigned int i = 0;i < size;i++)
				{
					counters[i] = new CommandDrawNumber(script);
		
					counters[i]->startX = counterX + (!vertical ? spacing*i : 0);
					counters[i]->y = counterY + (vertical ? spacing*i : 0);
					counters[i]->normalTranslation = translation;
					counters[i]->font = font;
					counters[i]->spacing = fontSpacing;
					counters[i]->whenNotZero = !alwaysShowCounter;
					counters[i]->drawValue = counters[i]->value = CommandDrawNumber::CONSTANT;
					counters[i]->length = INT_MAX;
					counters[i]->shadow = shadow;
					counters[i]->shadowX = shadowX;
					counters[i]->shadowY = shadowY;
				}
			}
		
			for(unsigned int i = 0;i < size;i++)
				counters[i]->Tick(block, statusBar, hudChanged);
		}
	protected:
		enum Styles
		{
			STYLE_Doom,
			STYLE_Heretic,
			STYLE_Hexen,
			STYLE_HexenStrict,
			STYLE_Strife
		};

		Styles				style;
		unsigned int		size;
		bool				alwaysShow;
		bool				noArtibox;
		bool				noArrows;
		bool				alwaysShowCounter;
		bool				translucent;
		bool				vertical;
		bool				shadow;
		int					shadowX;
		int					shadowY;
		SBarInfoCoordinate	x;
		SBarInfoCoordinate	y;
		CommandDrawNumber*	*counters;
	private:
		// This information will be transferred to counters as soon as possible.
		FFont				*font;
		SBarInfoCoordinate	counterX;
		SBarInfoCoordinate	counterY;
		EColorRange			translation;
		int					fontSpacing;
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawKeyBar : public SBarInfoCommand
{
	public:
		CommandDrawKeyBar(SBarInfo *script) : SBarInfoCommand(script),
			number(3), vertical(false), reverse(false), reverseRows(false),
			iconSize(-1), rowIconSize(-1), keyOffset(0), rowSize(0)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			AInventory *item = statusBar->CPlayer->mo->Inventory;
			if(item == NULL)
				return;
			int slotOffset = 0;
			int rowOffset = 0;
			int rowWidth = 0;
			for(unsigned int i = 0;i < number+keyOffset;i++)
			{
				while(!item->Icon.isValid() || !item->IsKindOf(RUNTIME_CLASS(AKey)))
				{
					item = item->Inventory;
					if(item == NULL)
						return;
				}
				if(i >= keyOffset) //Should we start drawing?
				{
					if(!vertical)
					{
						statusBar->DrawGraphic(TexMan[item->Icon], x+slotOffset, y+rowOffset, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
						rowWidth = rowIconSize == -1 ? TexMan[item->Icon]->GetScaledHeight()+2 : rowIconSize;
					}
					else
					{
						statusBar->DrawGraphic(TexMan[item->Icon], x+rowOffset, y+slotOffset, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
						rowWidth = rowIconSize == -1 ? TexMan[item->Icon]->GetScaledWidth()+2 : rowIconSize;
					}
		
					// If cmd.special is -1 then the slot size is auto detected
					if(iconSize == -1)
					{
						if(!vertical)
							slotOffset += (reverse ? -1 : 1) * (TexMan[item->Icon]->GetScaledWidth() + 2);
						else
							slotOffset += (reverse ? -1 : 1) * (TexMan[item->Icon]->GetScaledHeight() + 2);
					}
					else
						slotOffset += (reverse ? -iconSize : iconSize);
		
					if(rowSize > 0 && (i % rowSize == rowSize-1))
					{
						if(reverseRows)
							rowOffset -= rowWidth;
						else
							rowOffset += rowWidth;
						rowWidth = 0;
						slotOffset = 0;
					}
				}
		
				item = item->Inventory;
				if(item == NULL)
					return;
			}
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_IntConst);
			number = sc.Number;
			sc.MustGetToken(',');
			sc.MustGetToken(TK_Identifier);
			if(sc.Compare("vertical"))
				vertical = true;
			else if(!sc.Compare("horizontal"))
				sc.ScriptError("Unknown direction '%s'.", sc.String);
			sc.MustGetToken(',');
			while(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("reverserows"))
					reverseRows = true;
				else if(sc.Compare("reverse"))
					reverse = true;
				else
					sc.ScriptError("Unknown flag '%s'.", sc.String);
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
			if(sc.CheckToken(TK_Auto))
				iconSize = -1;
			else
			{
				sc.MustGetToken(TK_IntConst);
				iconSize = sc.Number;
			}
			sc.MustGetToken(',');
			GetCoordinates(sc, fullScreenOffsets, x, y);
			if(sc.CheckToken(','))
			{
				//key offset
				sc.MustGetToken(TK_IntConst);
				keyOffset = sc.Number;
				if(sc.CheckToken(','))
				{
					//max per row/column
					sc.MustGetToken(TK_IntConst);
					rowSize = sc.Number;
					sc.MustGetToken(',');
					//row/column spacing (opposite of previous)
					if(sc.CheckToken(TK_Auto))
						rowIconSize = -1;
					else
					{
						sc.MustGetToken(TK_IntConst);
						rowIconSize = sc.Number;
					}
				}
			}
			sc.MustGetToken(';');
		}
	protected:
		unsigned int		number;
		bool				vertical;
		bool				reverse;
		bool				reverseRows;
		int					iconSize;
		int					rowIconSize;
		unsigned int		keyOffset;
		unsigned int		rowSize;
		SBarInfoCoordinate	x;
		SBarInfoCoordinate	y;
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawBar : public SBarInfoCommand
{
	public:
		CommandDrawBar(SBarInfo *script) : SBarInfoCommand(script),
			border(0), horizontal(false), reverse(false), foreground(-1),
			background(-1), type(HEALTH), interpolationSpeed(0), drawValue(0),
			pixel(-1)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			if(foreground == -1 || statusBar->Images[foreground] == NULL)
				return; //don't draw anything.
			assert(statusBar->Images[foreground] != NULL);
			
			FTexture *fg = statusBar->Images[foreground];
			FTexture *bg = (background != -1) ? statusBar->Images[background] : NULL;

			fixed_t value = drawValue;
			if(border != 0)
			{
				value = FRACUNIT - value; //invert since the new drawing method requires drawing the bg on the fg.

				//Draw the whole foreground
				statusBar->DrawGraphic(fg, this->x, this->y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
			}
			else
			{
				// Draw background
				if (bg != NULL && bg->GetScaledWidth() == fg->GetScaledWidth() && bg->GetScaledHeight() == fg->GetScaledHeight())
					statusBar->DrawGraphic(bg, this->x, this->y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
				else
					statusBar->DrawGraphic(fg, this->x, this->y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, false, 0, false, -1, -1, 0, 0, 0, 0, true);
			}
		
			// {cx, cy, cr, cb}
			fixed_t clip[4] = {0, 0, 0, 0};
		
			fixed_t sizeOfImage = (horizontal ? fg->GetScaledWidth()-border*2 : fg->GetScaledHeight()-border*2)<<FRACBITS;
			clip[(!horizontal)|((horizontal ? !reverse : reverse)<<1)] = sizeOfImage - FixedMul(sizeOfImage, value);
			// Draw background
			if(border != 0)
			{
				for(unsigned int i = 0;i < 4;i++)
					clip[i] += border<<FRACBITS;
		
				if (bg != NULL && bg->GetScaledWidth() == fg->GetScaledWidth() && bg->GetScaledHeight() == fg->GetScaledHeight())
					statusBar->DrawGraphic(bg, this->x, this->y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, false, 0, false, -1, -1, clip[0], clip[1], clip[2], clip[3]);
				else
					statusBar->DrawGraphic(fg, this->x, this->y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, false, 0, false, -1, -1, clip[0], clip[1], clip[2], clip[3], true);
			}
			else
				statusBar->DrawGraphic(fg, this->x, this->y, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), false, false, 0, false, -1, -1, clip[0], clip[1], clip[2], clip[3]);
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_StringConst);
			foreground = script->newImage(sc.String);
			sc.MustGetToken(',');
			sc.MustGetToken(TK_StringConst);
			background = script->newImage(sc.String);
			sc.MustGetToken(',');
			sc.MustGetToken(TK_Identifier);
			if(sc.Compare("health"))
			{
				type = HEALTH;
				ParseComparator(sc);
			}
			else if(sc.Compare("armor"))
			{
				type = ARMOR;
				ParseComparator(sc);
			}
			else if(sc.Compare("ammo1"))
				type = AMMO1;
			else if(sc.Compare("ammo2"))
				type = AMMO2;
			else if(sc.Compare("ammo")) //request the next string to be an ammo type
			{
				bool parenthesized = sc.CheckToken('(');

				if(!parenthesized || !sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				type = AMMO;
				data.inventoryItem = PClass::FindClass(sc.String);
				if(data.inventoryItem == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(data.inventoryItem)) //must be a kind of ammo
				{
					sc.ScriptMessage("'%s' is not a type of ammo.", sc.String);
					data.inventoryItem = RUNTIME_CLASS(AAmmo);
				}

				if(parenthesized) sc.MustGetToken(')');
			}
			else if(sc.Compare("frags"))
				type = FRAGS;
			else if(sc.Compare("kills"))
				type = KILLS;
			else if(sc.Compare("items"))
				type = ITEMS;
			else if(sc.Compare("secrets"))
				type = SECRETS;
			else if(sc.Compare("airtime"))
				type = AIRTIME;
			else if(sc.Compare("savepercent"))
				type = SAVEPERCENT;
			else if(sc.Compare("poweruptime"))
			{
				bool parenthesized = sc.CheckToken('(');

				type = POWERUPTIME;
				if(!parenthesized || !sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				data.inventoryItem = PClass::FindClass(sc.String);
				if(data.inventoryItem == NULL || !RUNTIME_CLASS(APowerupGiver)->IsAncestorOf(data.inventoryItem))
				{
					sc.ScriptMessage("'%s' is not a type of PowerupGiver.", sc.String);
					data.inventoryItem = RUNTIME_CLASS(APowerupGiver);
				}

				if(parenthesized) sc.MustGetToken(')');
			}
			else
			{
				type = INVENTORY;
				data.inventoryItem = PClass::FindClass(sc.String);
				if(data.inventoryItem == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(data.inventoryItem))
				{
					sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
					data.inventoryItem = RUNTIME_CLASS(AInventory);
				}
			}
			sc.MustGetToken(',');
			sc.MustGetToken(TK_Identifier);
			if(sc.Compare("horizontal"))
				horizontal = true;
			else if(!sc.Compare("vertical"))
				sc.ScriptError("Unknown direction '%s'.", sc.String);
			sc.MustGetToken(',');
			while(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("reverse"))
					reverse = true;
				else if(sc.Compare("interpolate"))
				{
					sc.MustGetToken('(');
					sc.MustGetToken(TK_IntConst);
					interpolationSpeed = sc.Number;
					sc.MustGetToken(')');
				}
				else
					sc.ScriptError("Unkown flag '%s'.", sc.String);
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
			GetCoordinates(sc, fullScreenOffsets, x, y);
			if(sc.CheckToken(',')) //border
			{
				sc.MustGetToken(TK_IntConst);
				border = sc.Number;

				// Flip the direction since it represents the area to clip
				if(border != 0)
					reverse = !reverse;
			}
			sc.MustGetToken(';');
		
			if(type == HEALTH)
				interpolationSpeed = script->interpolateHealth ? script->interpolationSpeed : interpolationSpeed;
			else if(type == ARMOR)
				interpolationSpeed = script->interpolateArmor ? script->armorInterpolationSpeed : interpolationSpeed;
		}
		void	Reset()
		{
			drawValue = 0;
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			fixed_t value = 0;
			int max = 0;
			switch(type)
			{
				case HEALTH:
					value = statusBar->CPlayer->mo->health;
					if(value < 0) //health shouldn't display negatives
						value = 0;
		
					if(data.useMaximumConstant)
						max = data.value;
					else if(data.inventoryItem != NULL)
					{
						AInventory *item = statusBar->CPlayer->mo->FindInventory(data.inventoryItem); //max comparer
						if(item != NULL)
							max = item->Amount;
						else
							max = 0;
					}
					else //default to the class's health
						max = statusBar->CPlayer->mo->GetMaxHealth() + statusBar->CPlayer->mo->stamina;
					break;
				case ARMOR:
					value = statusBar->armor != NULL ? statusBar->armor->Amount : 0;
					if(data.useMaximumConstant)
						max = data.value;
					else if(data.inventoryItem != NULL)
					{
						AInventory *item = statusBar->CPlayer->mo->FindInventory(data.inventoryItem);
						if(item != NULL)
							max = item->Amount;
						else
							max = 0;
					}
					else
						max = 100;
					break;
				case AMMO1:
					value = statusBar->ammocount1;
					if(statusBar->ammo1 == NULL) //no ammo, draw as empty
					{
						value = 0;
						max = 1;
					}
					else
						max = statusBar->ammo1->MaxAmount;
					break;
				case AMMO2:
					value = statusBar->ammocount2;
					if(statusBar->ammo2 == NULL) //no ammo, draw as empty
					{
						value = 0;
						max = 1;
					}
					else
						max = statusBar->ammo2->MaxAmount;
					break;
				case AMMO:
				{
					AInventory *item = statusBar->CPlayer->mo->FindInventory(data.inventoryItem);
					if(item != NULL)
					{
						value = item->Amount;
						max = item->MaxAmount;
					}
					else
						value = 0;
					break;
				}
				case FRAGS:
					value = statusBar->CPlayer->fragcount;
					max = fraglimit;
					break;
				case KILLS:
					value = level.killed_monsters;
					max = level.total_monsters;
					break;
				case ITEMS:
					value = level.found_items;
					max = level.total_items;
					break;
				case SECRETS:
					value = level.found_secrets;
					max = level.total_secrets;
					break;
				case INVENTORY:
				{
					AInventory *item = statusBar->CPlayer->mo->FindInventory(data.inventoryItem);
					if(item != NULL)
					{
						value = item->Amount;
						max = item->MaxAmount;
					}
					else
						value = 0;
					break;
				}
				case AIRTIME:
					value = clamp<int>(statusBar->CPlayer->air_finished - level.time, 0, INT_MAX);
					max = level.airsupply;
					break;
				case POWERUPTIME:
				{
					//Get the PowerupType and check to see if the player has any in inventory.
					APowerupGiver *powerupGiver = (APowerupGiver*) GetDefaultByType(data.inventoryItem);
					const PClass *powerupType = powerupGiver->PowerupType;
					APowerup *powerup = (APowerup*) statusBar->CPlayer->mo->FindInventory(powerupType);
					if(powerup != NULL && powerupType != NULL && powerupGiver != NULL)
					{
						value = powerup->EffectTics + 1;
						if(powerupGiver->EffectTics == 0) //if 0 we need to get the default from the powerup
							max = ((APowerup*) GetDefaultByType(powerupType))->EffectTics + 1;
						else
							max = powerupGiver->EffectTics + 1;
					}
					break;
				}
				case SAVEPERCENT:
				{
					AHexenArmor *harmor = statusBar->CPlayer->mo->FindInventory<AHexenArmor>();
					if(harmor != NULL)
					{
						value = harmor->Slots[0] + harmor->Slots[1] +
							harmor->Slots[2] + harmor->Slots[3] + harmor->Slots[4];
					}
					//Hexen counts basic armor also so we should too.
					if(statusBar->armor != NULL)
					{
						value += FixedMul(statusBar->armor->SavePercent, 100*FRACUNIT);
					}
					value >>= FRACBITS;
					max = 100;
					break;
				}
				default: return;
			}

			if(max != 0 && value > 0)
			{
				value = (value << FRACBITS) / max;
				if(value > FRACUNIT)
					value = FRACUNIT;
			}
			else
				value = 0;
			if(interpolationSpeed != 0 && (!hudChanged || level.time == 1))
			{
				// [BL] Since we used a percentage (in order to get the most fluid animation)
				//      we need to establish a cut off point so the last pixel won't hang as the animation slows
				if(pixel == -1 && statusBar->Images[foreground])
					pixel = MAX(1, FRACUNIT/statusBar->Images[foreground]->GetWidth());

				if(abs(drawValue - value) < pixel)
					drawValue = value;
				else if(value < drawValue)
					drawValue -= clamp<fixed_t>((drawValue - value) >> 2, 1, FixedDiv(interpolationSpeed<<FRACBITS, FRACUNIT*100));
				else if(drawValue < value)
					drawValue += clamp<fixed_t>((value - drawValue) >> 2, 1, FixedDiv(interpolationSpeed<<FRACBITS, FRACUNIT*100));
			}
			else
				drawValue = value;
		}
	protected:
		void	ParseComparator(FScanner &sc)
		{
			bool extendedSyntax = sc.CheckToken('(');

			if(sc.CheckToken(TK_Identifier) || (extendedSyntax && sc.CheckToken(TK_StringConst))) //comparing reference
			{
				data.inventoryItem = PClass::FindClass(sc.String);
				if(data.inventoryItem == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(data.inventoryItem)) //must be a kind of inventory
				{
					sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
					data.inventoryItem = RUNTIME_CLASS(AInventory);
				}
			}
			else if(extendedSyntax && sc.CheckToken(TK_IntConst))
			{
				data.useMaximumConstant = true;
				data.value = sc.Number;
			}

			if(extendedSyntax)
				sc.MustGetToken(')');
		}

		enum ValueType
		{
			HEALTH,
			ARMOR,
			AMMO1,
			AMMO2,
			AMMO,
			FRAGS,
			INVENTORY,
			KILLS,
			ITEMS,
			SECRETS,
			ARMORCLASS,
			POWERUPTIME,
			AIRTIME,
			SAVEPERCENT
		};

		struct AdditionalData
		{
		public:
			AdditionalData() : useMaximumConstant(false)
			{
				inventoryItem = NULL;
			}

			bool	useMaximumConstant;
			union
			{
				const PClass	*inventoryItem;
				int				value;
			};
		};

		unsigned int		border;
		bool				horizontal;
		bool				reverse;
		int					foreground;
		int					background;
		ValueType			type;
		AdditionalData		data;
		SBarInfoCoordinate	x;
		SBarInfoCoordinate	y;

		int					interpolationSpeed;
		fixed_t				drawValue;
		fixed_t				pixel;
};

////////////////////////////////////////////////////////////////////////////////

class CommandIsSelected : public SBarInfoCommandFlowControl
{
	public:
		CommandIsSelected(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			negate(false)
		{
			weapon[0] = NULL;
			weapon[1] = NULL;
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("not"))
				{
					negate = true;
					if(!sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				}
			}
			else
				sc.MustGetToken(TK_StringConst);
			for(int i = 0;i < 2;i++)
			{
				weapon[i] = PClass::FindClass(sc.String);
				if(weapon[i] == NULL || !RUNTIME_CLASS(AWeapon)->IsAncestorOf(weapon[i]))
				{
					sc.ScriptMessage("'%s' is not a type of weapon.", sc.String);
					weapon[i] = RUNTIME_CLASS(AWeapon);
				}
		
				if(sc.CheckToken(','))
				{
					if(!sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				}
				else
					break;
			}
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			if(statusBar->CPlayer->ReadyWeapon != NULL)
			{
				const PClass *readyWeapon = statusBar->CPlayer->ReadyWeapon->GetClass();
				SetTruth(((weapon[1] != NULL) &&
						((negate && (weapon[0] != readyWeapon && weapon[1] != readyWeapon)) ||
						(!negate && (weapon[0] == readyWeapon || weapon[1] == readyWeapon)))) ||
					((weapon[1] == NULL) &&
						((!negate && weapon[0] == readyWeapon) || (negate && weapon[0] != readyWeapon))), block, statusBar);
			}
		}
	protected:
		bool			negate;
		const PClass	*weapon[2];
};

////////////////////////////////////////////////////////////////////////////////

class CommandPlayerClass : public SBarInfoCommandFlowControl
{
	public:
		CommandPlayerClass(SBarInfo *script) : SBarInfoCommandFlowControl(script)
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_Identifier);
			do
			{
				bool foundClass = false;
				for(unsigned int c = 0;c < PlayerClasses.Size();c++)
				{
					if(stricmp(sc.String, PlayerClasses[c].Type->Meta.GetMetaString(APMETA_DisplayName)) == 0)
					{
						foundClass = true;
						classes.Push(PlayerClasses[c].Type->ClassIndex);
						break;
					}
				}
				/*
				if(!foundClass)
					sc.ScriptError("Unkown PlayerClass '%s'.", sc.String);
				*/
				if(!sc.CheckToken(','))
					break;
			}
			while(sc.CheckToken(TK_Identifier));
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			if(statusBar->CPlayer->cls == NULL)
				return; //No class so we can not continue
		
			int spawnClass = statusBar->CPlayer->cls->ClassIndex;
			for(unsigned int i = 0;i < classes.Size();i++)
			{
				if(classes[i] == spawnClass)
				{
					SetTruth(true, block, statusBar);
					return;
				}
			}
			SetTruth(false, block, statusBar);
		}
	protected:
		TArray<int>	classes;
};

////////////////////////////////////////////////////////////////////////////////

class CommandPlayerType : public SBarInfoCommandFlowControl
{
	public:
		CommandPlayerType(SBarInfo *script) : SBarInfoCommandFlowControl(script)
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			do
			{
				bool foundClass = false;
				const PClass *cls = PClass::FindClass(sc.String);
				if (cls != NULL)
				{
					foundClass = true;
					classes.Push(cls);
				}
				/*
				if(!foundClass)
					sc.ScriptError("Unkown PlayerClass '%s'.", sc.String);
				*/
				if(!sc.CheckToken(','))
					break;
			}
			while(sc.CheckToken(TK_Identifier) || sc.CheckToken(TK_StringConst));
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			if(statusBar->CPlayer->cls == NULL)
				return; //No class so we can not continue
		
			for(unsigned int i = 0;i < classes.Size();i++)
			{
				if (statusBar->CPlayer->cls->IsDescendantOf(classes[i]))
				{
					SetTruth(true, block, statusBar);
					return;
				}
			}
			SetTruth(false, block, statusBar);
		}
	protected:
		TArray<const PClass *> classes;
};

////////////////////////////////////////////////////////////////////////////////

class CommandHasWeaponPiece : public SBarInfoCommandFlowControl
{
	public:
		CommandHasWeaponPiece(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			weapon(NULL), piece(1)
		{
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			weapon = PClass::FindClass(sc.String);
			if(weapon == NULL || !RUNTIME_CLASS(AWeapon)->IsAncestorOf(weapon)) //must be a weapon
			{
				sc.ScriptMessage("%s is not a kind of weapon.", sc.String);
				weapon = RUNTIME_CLASS(AWeapon);
			}
			sc.MustGetToken(',');
			sc.MustGetToken(TK_IntConst);
			if(sc.Number < 1)
				sc.ScriptError("Weapon piece number can not be less than 1.");
			piece = sc.Number;
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			for(AInventory *inv = statusBar->CPlayer->mo->Inventory;inv != NULL;inv=inv->Inventory)
			{
				if(inv->IsKindOf(RUNTIME_CLASS(AWeaponHolder)))
				{
					AWeaponHolder *hold = static_cast<AWeaponHolder*>(inv);
					if(hold->PieceWeapon == weapon)
					{
						if(hold->PieceMask & (1 << (piece-1)))
							SetTruth(true, block, statusBar);
						return;
					}
				}
			}
			SetTruth(false, block, statusBar);
		}
	protected:
		const PClass	*weapon;
		unsigned int	piece;
};

////////////////////////////////////////////////////////////////////////////////

class CommandDrawGem : public SBarInfoCommand
{
	public:
		CommandDrawGem(SBarInfo *script) : SBarInfoCommand(script),
			wiggle(false), translatable(false), armor(false), reverse(false),
			chain(-1), gem(-1), leftPadding(0), rightPadding(0), chainSize(1),
			interpolationSpeed(0), drawValue(0), goalValue(0), chainWiggle(0)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			FTexture *chainImg = statusBar->Images[chain];
			FTexture *gemImg = statusBar->Images[gem];
			if(chainImg == NULL)
				return;
		
			SBarInfoCoordinate drawY = y;
			if(wiggle && drawValue != goalValue) // Should only wiggle when the value doesn't equal what is being drawn.
				drawY += chainWiggle;
			int chainWidth = chainImg->GetScaledWidth();
			int offset = (int) (((double) (chainWidth-leftPadding-rightPadding)/100)*drawValue);
			statusBar->DrawGraphic(chainImg, x+(offset%chainSize), drawY, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets());
			if(gemImg != NULL)
				statusBar->DrawGraphic(gemImg, x+leftPadding+offset, drawY, block->XOffset(), block->YOffset(), block->Alpha(), block->FullScreenOffsets(), translatable);
		}
		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			while(sc.CheckToken(TK_Identifier))
			{
				if(sc.Compare("wiggle"))
					wiggle = true;
				else if(sc.Compare("translatable"))
					translatable = true;
				else if(sc.Compare("armor"))
					armor = true;
				else if(sc.Compare("interpolate"))
				{
					sc.MustGetToken('(');
					sc.MustGetToken(TK_IntConst);
					interpolationSpeed = sc.Number;
					sc.MustGetToken(')');
				}
				else if(sc.Compare("reverse"))
					reverse = true;
				else
					sc.ScriptError("Unknown drawgem flag '%s'.", sc.String);
				if(!sc.CheckToken('|'))
					sc.MustGetToken(',');
			}
			sc.MustGetToken(TK_StringConst); //chain
			chain = script->newImage(sc.String);
			sc.MustGetToken(',');
			sc.MustGetToken(TK_StringConst); //gem
			gem = script->newImage(sc.String);
			sc.MustGetToken(',');
			bool negative = sc.CheckToken('-');
			sc.MustGetToken(TK_IntConst);
			leftPadding = (negative ? -1 : 1) * sc.Number;
			sc.MustGetToken(',');
			negative = sc.CheckToken('-');
			sc.MustGetToken(TK_IntConst);
			rightPadding = (negative ? -1 : 1) * sc.Number;
			sc.MustGetToken(',');
			sc.MustGetToken(TK_IntConst);
			if(sc.Number < 0)
				sc.ScriptError("Chain size must be a positive number.");
			chainSize = sc.Number+1;
			sc.MustGetToken(',');
			GetCoordinates(sc, fullScreenOffsets, x, y);
			sc.MustGetToken(';');
		
			if(!armor)
				interpolationSpeed = script->interpolateHealth ? script->interpolationSpeed : interpolationSpeed;
			else
				interpolationSpeed = script->interpolateArmor ? script->armorInterpolationSpeed : interpolationSpeed;
		}
		void	Reset()
		{
			drawValue = 0;
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			goalValue = armor ? (statusBar->armor ? statusBar->armor->Amount : 0) : statusBar->CPlayer->mo->health;
			int max = armor ? 100 : statusBar->CPlayer->mo->GetMaxHealth() + statusBar->CPlayer->mo->stamina;
			if(max != 0 && goalValue > 0)
			{
				goalValue = (goalValue*100)/max;
				if(goalValue > 100)
					goalValue = 100;
			}
			else
				goalValue = 0;
		
			goalValue = reverse ? 100 - goalValue : goalValue;
		
			if(interpolationSpeed != 0 && (!hudChanged || level.time == 1)) // At the start force an animation
			{
				if(goalValue < drawValue)
					drawValue -= clamp<int>((drawValue - goalValue) >> 2, 1, interpolationSpeed);
				else if(drawValue < goalValue)
					drawValue += clamp<int>((goalValue - drawValue) >> 2, 1, interpolationSpeed);
			}
			else
				drawValue = goalValue;
		
			if(wiggle && level.time & 1)
				chainWiggle = pr_chainwiggle() & 1;
		}
	protected:
		bool				wiggle;
		bool				translatable;
		bool				armor;
		bool				reverse;
		int					chain;
		int					gem;
		int					leftPadding;
		int					rightPadding;
		unsigned int		chainSize;
		SBarInfoCoordinate	x;
		SBarInfoCoordinate	y;

		int					interpolationSpeed;
		int					drawValue;
		int					goalValue;
	private:
		int					chainWiggle;
		static FRandom		pr_chainwiggle;
};
FRandom CommandDrawGem::pr_chainwiggle; //use the same method of chain wiggling as heretic.

////////////////////////////////////////////////////////////////////////////////

class CommandWeaponAmmo : public SBarInfoCommandFlowControl
{
	public:
		CommandWeaponAmmo(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			conditionAnd(false), negate(false)
		{
			ammo[0] = NULL;
			ammo[1] = NULL;
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			if(sc.Compare("not") && sc.TokenType == TK_Identifier)
			{
				negate = true;
				if(!sc.CheckToken(TK_StringConst))
					sc.MustGetToken(TK_Identifier);
			}
			for(int i = 0;i < 2;i++)
			{
				ammo[i] = PClass::FindClass(sc.String);
				if(ammo[i] == NULL || !RUNTIME_CLASS(AAmmo)->IsAncestorOf(ammo[i])) //must be a kind of ammo
				{
					sc.ScriptMessage("'%s' is not a type of ammo.", sc.String);
					ammo[i] = RUNTIME_CLASS(AAmmo);
				}
		
				if(sc.CheckToken(TK_OrOr))
				{
					conditionAnd = false;
					if(!sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				}
				else if(sc.CheckToken(TK_AndAnd))
				{
					conditionAnd = true;
					if(!sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				}
				else
					break;
			}
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			if(statusBar->CPlayer->ReadyWeapon != NULL)
			{
				const PClass *AmmoType1 = statusBar->CPlayer->ReadyWeapon->AmmoType1;
				const PClass *AmmoType2 = statusBar->CPlayer->ReadyWeapon->AmmoType2;
				bool usesammo1 = (AmmoType1 != NULL);
				bool usesammo2 = (AmmoType2 != NULL);
				if(negate && !usesammo1 && !usesammo2) //if the weapon doesn't use ammo don't go though the trouble.
				{
					SetTruth(true, block, statusBar);
					return;
				}
				//Or means only 1 ammo type needs to match and means both need to match.
				if(ammo[1] != NULL)
				{
					bool match1 = ((usesammo1 && (AmmoType1 == ammo[0] || AmmoType1 == ammo[1])) || !usesammo1);
					bool match2 = ((usesammo2 && (AmmoType2 == ammo[0] || AmmoType2 == ammo[1])) || !usesammo2);
					if((!conditionAnd && (match1 || match2)) || (conditionAnd && (match1 && match2)))
					{
						if(!negate)
						{
							SetTruth(true, block, statusBar);
							return;
						}
					}
					else if(negate)
					{
						SetTruth(true, block, statusBar);
						return;
					}
				}
				else //Every thing here could probably be one long if statement but then it would be more confusing.
				{
					if((usesammo1 && (AmmoType1 == ammo[0])) || (usesammo2 && (AmmoType2 == ammo[0])))
					{
						if(!negate)
						{
							SetTruth(true, block, statusBar);
							return;
						}
					}
					else if(negate)
					{
						SetTruth(true, block, statusBar);
						return;
					}
				}
			}
			SetTruth(false, block, statusBar);
		}
	protected:
		bool			conditionAnd;
		bool			negate;
		const PClass	*ammo[2];
};

////////////////////////////////////////////////////////////////////////////////

class CommandInInventory : public SBarInfoCommandFlowControl
{
	public:
		CommandInInventory(SBarInfo *script) : SBarInfoCommandFlowControl(script),
			conditionAnd(false), negate(false)
		{
			item[0] = item[1] = NULL;
			amount[0] = amount[1] = 0;
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			if(!sc.CheckToken(TK_StringConst))
				sc.MustGetToken(TK_Identifier);
			if(sc.Compare("not") && sc.TokenType == TK_Identifier)
			{
				negate = true;
				if(!sc.CheckToken(TK_StringConst))
					sc.MustGetToken(TK_Identifier);
			}
			for(int i = 0;i < 2;i++)
			{
				item[i] = PClass::FindClass(sc.String);
				if(item[i] == NULL || !RUNTIME_CLASS(AInventory)->IsAncestorOf(item[i]))
				{
					sc.ScriptMessage("'%s' is not a type of inventory item.", sc.String);
					item[i] = RUNTIME_CLASS(AInventory);
				}
		
				if (sc.CheckToken(','))
				{
					sc.MustGetNumber();
					amount[i] = sc.Number;
				}
		
				if(sc.CheckToken(TK_OrOr))
				{
					conditionAnd = false;
					if(!sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				}
				else if(sc.CheckToken(TK_AndAnd))
				{
					conditionAnd = true;
					if(!sc.CheckToken(TK_StringConst))
						sc.MustGetToken(TK_Identifier);
				}
				else
					break;
			}
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
		void	Tick(const SBarInfoMainBlock *block, const DSBarInfo *statusBar, bool hudChanged)
		{
			SBarInfoCommandFlowControl::Tick(block, statusBar, hudChanged);

			AInventory *invItem[2] = { statusBar->CPlayer->mo->FindInventory(item[0]), statusBar->CPlayer->mo->FindInventory(item[1]) };
			if (invItem[0] != NULL && amount[0] > 0 && invItem[0]->Amount < amount[0]) invItem[0] = NULL;
			if (invItem[1] != NULL && amount[1] > 0 && invItem[1]->Amount < amount[1]) invItem[1] = NULL;
			if(invItem[1] != NULL && conditionAnd)
			{
				if((invItem[0] != NULL && invItem[1] != NULL) && !negate)
				{
					SetTruth(true, block, statusBar);
					return;
				}
				else if((invItem[0] == NULL || invItem[1] == NULL) && negate)
				{
					SetTruth(true, block, statusBar);
					return;
				}
			}
			else if(invItem[1] != NULL && !conditionAnd)
			{
				if((invItem[0] != NULL || invItem[1] != NULL) && !negate)
				{
					SetTruth(true, block, statusBar);
					return;
				}
				else if((invItem[0] == NULL && invItem[1] == NULL) && negate)
				{
					SetTruth(true, block, statusBar);
					return;
				}
			}
			else if((invItem[0] != NULL) && !negate)
			{
				SetTruth(true, block, statusBar);
				return;
			}
			else if((invItem[0] == NULL) && negate)
			{
				SetTruth(true, block, statusBar);
				return;
			}
			SetTruth(false, block, statusBar);
		}
	protected:
		bool			conditionAnd;
		bool			negate;
		const PClass	*item[2];
		int				amount[2];
};

////////////////////////////////////////////////////////////////////////////////

class CommandAlpha : public SBarInfoMainBlock
{
	public:
		CommandAlpha(SBarInfo *script) : SBarInfoMainBlock(script)
		{
		}

		void	Draw(const SBarInfoMainBlock *block, const DSBarInfo *statusBar)
		{
			forceScaled = block->ForceScaled();
			fullScreenOffsets = block->FullScreenOffsets();

			SBarInfoMainBlock::Draw(block, statusBar, block->XOffset(), block->YOffset(), block->Alpha());
		}

		void	Parse(FScanner &sc, bool fullScreenOffsets)
		{
			sc.MustGetToken(TK_FloatConst);
			alpha = fixed_t(FRACUNIT * sc.Float);

			// We don't want to allow all the options of a regular main block
			// so skip to the SBarInfoCommandFlowControl.
			SBarInfoCommandFlowControl::Parse(sc, fullScreenOffsets);
		}
};

////////////////////////////////////////////////////////////////////////////////

static const char *SBarInfoCommandNames[] =
{
	"drawimage", "drawnumber", "drawswitchableimage",
	"drawmugshot", "drawselectedinventory",
	"drawinventorybar", "drawbar", "drawgem",
	"drawshader", "drawstring", "drawkeybar",
	"gamemode", "playerclass", "playertype", "aspectratio",
	"isselected", "usesammo", "usessecondaryammo",
	"hasweaponpiece", "inventorybarnotvisible",
	"weaponammo", "ininventory", "alpha",
	NULL
};

enum SBarInfoCommands
{
	SBARINFO_DRAWIMAGE, SBARINFO_DRAWNUMBER, SBARINFO_DRAWSWITCHABLEIMAGE,
	SBARINFO_DRAWMUGSHOT, SBARINFO_DRAWSELECTEDINVENTORY,
	SBARINFO_DRAWINVENTORYBAR, SBARINFO_DRAWBAR, SBARINFO_DRAWGEM,
	SBARINFO_DRAWSHADER, SBARINFO_DRAWSTRING, SBARINFO_DRAWKEYBAR,
	SBARINFO_GAMEMODE, SBARINFO_PLAYERCLASS, SBARINFO_PLAYERTYPE, SBARINFO_ASPECTRATIO,
	SBARINFO_ISSELECTED, SBARINFO_USESAMMO, SBARINFO_USESSECONDARYAMMO,
	SBARINFO_HASWEAPONPIECE, SBARINFO_INVENTORYBARNOTVISIBLE,
	SBARINFO_WEAPONAMMO, SBARINFO_ININVENTORY, SBARINFO_ALPHA,
};

SBarInfoCommand *SBarInfoCommandFlowControl::NextCommand(FScanner &sc)
{
	if(sc.CheckToken(TK_Identifier))
	{
		switch(sc.MatchString(SBarInfoCommandNames))
		{
			default: break;
			case SBARINFO_DRAWIMAGE: return new CommandDrawImage(script);
			case SBARINFO_DRAWSWITCHABLEIMAGE: return new CommandDrawSwitchableImage(script);
			case SBARINFO_DRAWSTRING: return new CommandDrawString(script);
			case SBARINFO_DRAWNUMBER: return new CommandDrawNumber(script);
			case SBARINFO_DRAWMUGSHOT: return new CommandDrawMugShot(script);
			case SBARINFO_DRAWSELECTEDINVENTORY: return static_cast<SBarInfoCommandFlowControl *> (new CommandDrawSelectedInventory(script));
			case SBARINFO_DRAWSHADER: return new CommandDrawShader(script);
			case SBARINFO_DRAWINVENTORYBAR: return new CommandDrawInventoryBar(script);
			case SBARINFO_DRAWKEYBAR: return new CommandDrawKeyBar(script);
			case SBARINFO_DRAWBAR: return new CommandDrawBar(script);
			case SBARINFO_DRAWGEM: return new CommandDrawGem(script);
			case SBARINFO_GAMEMODE: return new CommandGameMode(script);
			case SBARINFO_USESAMMO: return new CommandUsesAmmo(script);
			case SBARINFO_USESSECONDARYAMMO: return new CommandUsesSecondaryAmmo(script);
			case SBARINFO_INVENTORYBARNOTVISIBLE: return new CommandInventoryBarNotVisible(script);
			case SBARINFO_ASPECTRATIO: return new CommandAspectRatio(script);
			case SBARINFO_ISSELECTED: return new CommandIsSelected(script);
			case SBARINFO_PLAYERCLASS: return new CommandPlayerClass(script);
			case SBARINFO_PLAYERTYPE: return new CommandPlayerType(script);
			case SBARINFO_HASWEAPONPIECE: return new CommandHasWeaponPiece(script);
			case SBARINFO_WEAPONAMMO: return new CommandWeaponAmmo(script);
			case SBARINFO_ININVENTORY: return new CommandInInventory(script);
			case SBARINFO_ALPHA: return new CommandAlpha(script);
		}

		sc.ScriptError("Unknown command '%s'.\n", sc.String);
		return NULL;
	}

	sc.MustGetToken('}');
	return NULL;
}
