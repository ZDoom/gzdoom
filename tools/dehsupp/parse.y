%include{
#include <malloc.h>
#include "dehsupp.h"
}

%token_type {struct Token}

%token_destructor { if ($$.string) free($$.string); }

%left OR.
%left XOR.
%left AND.
%left MINUS PLUS.
%left MULTIPLY DIVIDE.
%left NEG.

%left EOI.

main ::= translation_unit.

translation_unit ::= .	/* empty */
translation_unit ::= translation_unit external_declaration.

external_declaration ::= print_statement.
external_declaration ::= actions_def.
external_declaration ::= org_heights_def.
external_declaration ::= action_list_def.
external_declaration ::= codep_conv_def.
external_declaration ::= org_spr_names_def.
external_declaration ::= state_map_def.
external_declaration ::= sound_map_def.
external_declaration ::= info_names_def.
external_declaration ::= thing_bits_def.
external_declaration ::= render_styles_def.

print_statement ::= PRINT LPAREN print_list RPAREN.
{
	printf ("\n");
}

print_list ::= .	/* EMPTY */
print_list ::= print_item.
print_list ::= print_item COMMA print_list.

print_item ::= STRING(A).			{ printf ("%s", A.string); }
print_item ::= exp(A).				{ printf ("%d", A); }
print_item ::= ENDL.				{ printf ("\n"); }

%type exp {int}
exp(A) ::= NUM(B).					{ A = B.val; }
exp(A) ::= exp(B) PLUS exp(C).		{ A = B + C; }
exp(A) ::= exp(B) MINUS exp(C).		{ A = B - C; }
exp(A) ::= exp(B) MULTIPLY exp(C).	{ A = B * C; }
exp(A) ::= exp(B) DIVIDE exp(C).	{ A = B / C; }
exp(A) ::= exp(B) OR exp(C).		{ A = B | C; }
exp(A) ::= exp(B) AND exp(C).		{ A = B & C; }
exp(A) ::= exp(B) XOR exp(C).		{ A = B ^ C; }
exp(A) ::= MINUS exp(B). [NEG]		{ A = -B; }
exp(A) ::= LPAREN exp(B) RPAREN.	{ A = B; }


actions_def ::= Actions LBRACE actions_list RBRACE SEMICOLON.

actions_list ::= .		/* empty */
actions_list ::= SYM(A).							{ AddAction (A.string); }
actions_list ::= actions_list COMMA SYM(A).			{ AddAction (A.string); }


org_heights_def ::= OrgHeights LBRACE org_heights_list RBRACE SEMICOLON.

org_heights_list ::= .	/* empty */
org_heights_list ::= exp(A).						{ AddHeight (A); }
org_heights_list ::= org_heights_list COMMA exp(A).	{ AddHeight (A); }


action_list_def ::= ActionList LBRACE action_list_list RBARCE SEMICOLON.

action_list_list ::= .	/* empty */
action_list_list ::= SYM(A).						{ AddActionMap (A.string); }
action_list_list ::= action_list_list COMMA SYM(A).	{ AddActionMap (A.string); }


codep_conv_def ::= CodePConv LBRACE codep_conv_list RBRACE SEMICOLON.

codep_conv_list ::= .		/* empty */
codep_conv_list ::= exp(A).							{ AddCodeP (A); }
codep_conv_list ::= codep_conv_list COMMA exp(A).	{ AddCodeP (A); }


org_spr_names_def ::= OrgSprNames LBRACE org_spr_names_list RBRACE SEMICOLON.

org_spr_names_list ::= .	/* empty */
org_spr_names_list ::= SYM(A).							{ AddSpriteName (A.string); }
org_spr_names_list ::= org_spr_names_list COMMA SYM(A).	{ AddSpriteName (A.string); }


state_map_def ::= StateMap LBRACE state_map_list RBRACE SEMICOLON.

state_map_list ::= .		/* empty */
state_map_list ::= state_map_entry.
state_map_list ::= state_map_list COMMA state_map_entry.

state_map_entry ::= SYM(A) COMMA state_type(B) COMMA exp(C). { AddStateMap (A.string, B, C); }

%type state_type {int}
state_type(A) ::= FirstState.		{ A = 0; }
state_type(A) ::= SpawnState.		{ A = 1; }
state_type(A) ::= DeathState.		{ A = 2; }


sound_map_def ::= SoundMap LBRACE sound_map_list RBRACE SEMICOLON.

sound_map_list ::= .		/* empty */
sound_map_list ::= STRING(A).						{ AddSoundMap (A.string); }
sound_map_list ::= sound_map_list COMMA STRING(A).	{ AddSoundMap (A.string); }


info_names_def ::= InfoNames LBRACE info_names_list RBRACE SEMICOLON.

info_names_list ::= .		/* empty */
info_names_list ::= SYM(A).							{ AddInfoName (A.string); }
info_names_list ::= info_names_list COMMA SYM(A).	{ AddInfoName (A.string); }


thing_bits_def ::= ThingBits LBRACE thing_bits_list RBRACE SEMICOLON.

thing_bits_list ::= .		/* empty */
thing_bits_list ::= thing_bits_entry.
thing_bits_list ::= thing_bits_list COMMA thing_bits_entry.

thing_bits_entry ::= exp(A) COMMA exp(B) COMMA SYM(C).	{ AddThingBits (C.string, A, B); }


render_styles_def ::= RenderStyles LBRACE render_styles_list RBRACE SEMICOLON.

render_styles_list ::= .	/* empty */
render_styles_list ::= render_styles_entry.
render_styles_list ::= render_styles_list COMMA render_styles_entry.

render_styles_entry ::= exp(A) COMMA SYM(B).		{ AddRenderStyle (B.string, A); }
