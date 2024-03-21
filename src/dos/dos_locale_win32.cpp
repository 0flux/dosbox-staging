/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if defined(WIN32)

#include "dos_locale.h"

#include "checks.h"

CHECK_NARROWING();

// XXX to be based on [MS-LCID]: Windows Language Code Identifier (LCID) Reference

/* XXX DOS keyboard layouts:

// XXX see also https://kbdlayout.info

us     // US (Standard, QWERTY/National)
ux     // US (International, QWERTY)
co     // US (Colemak)
dv     // US (Dvorak)
lh     // US (Left-Hand Dvorak)
rh     // US (Right-Hand Dvorak)
uk     // UK (Standard, QWERTY)
uk168  // UK (Alternate, QWERTY)
kx     // UK (International, QWERTY)
ar462  // Arabic (AZERTY/National)
ar470  // Arabic (QWERTY/National)
az     // Azeri (QWERTY/National)
ba     // Bosnian (QWERTZ)
be     // Belgian (AZERTY)
bx     // Belgian (International, AZERTY)
bg     // Bulgarian (QWERTY/National)
bg103  // Bulgarian (QWERTY/Phonetic)
bg241  // Bulgarian (JCUKEN/National)
bn     // Beninese (AZERTY)
br     // Brazilian (ABNT layout, QWERTY)
br274  // Brazilian (US layout, QWERTY)
by     // Belarusian (QWERTY/National)
ce     // Chechen (Standard, QWERTY/National)
ce443  // Chechen (Typewriter, QWERTY/National)
cf     // Canadian (Standard, QWERTY)
cf445  // Canadian (Dual-layer, QWERTY)
cg     // Montenegrin (QWERTZ)
cz     // Czech (QWERTZ)
cz243  // Czech (Standard, QWERTZ)
cz489  // Czech (Programmers, QWERTY)
de     // German (Standard, QWERTZ)
gr453  // German (Dual-layer, QWERTZ)
dk     // Danish (QWERTY)
ee     // Estonian (QWERTY)
es     // Spanish (QWERTY)
sx     // Spanish (International, QWERTY)
fi     // Finnish (QWERTY/ASERTT)
fo     // Faroese (QWERTY)
fr     // French (Standard, AZERTY)
fx     // French (International, AZERTY)
gk     // Greek (319, QWERTY/National)
gk220  // Greek (220, QWERTY/National)
gk459  // Greek (459, Non-Standard/National)
hr     // Croatian (QWERTZ/National)
hu     // Hungarian (101-key, QWERTY)
hu208  // Hungarian (102-key, QWERTY)
hy     // Armenian (QWERTY/National)
il     // Hebrew (QWERTY/National)
is     // Icelandic (101-key, QWERTY)
is161  // Icelandic (102-key, QWERTY)
it     // Italian (Standard, QWERTY/National)
it142  // Italian (142, QWERTY/National)
ix     // Italian (International, QWERTY)
// jp  // Japan
ka     // Georgian (QWERTY/National)
kk     // Kazakh (QWERTY/National)
ky     // Kyrgyz (QWERTY/National)
la     // Latin American (QWERTY)
lt     // Lithuanian (Baltic, QWERTY/Phonetic)
lt210  // Lithuanian (Programmers, QWERTY/Phonetic)
lt211  // Lithuanian (AZERTY/Phonetic)
lt221  // Lithuanian (LST 1582, AZERTY/Phonetic)
lt456  // Lithuanian (QWERTY/AZERTY/Phonetic)
lv     // Latvian (Standard, QWERTY/Phonetic)
lv455  // Latvian (QWERTY/UGJRMV/Phonetic)
mk     // Macedonian (QWERTZ/National)
mn     // Mongolian (QWERTY/National)
mt     // Maltese (UK layout, QWERTY)
mt103  // Maltese (US layout, QWERTY)
ne     // Nigerien (AZERTY)
ng     // Nigerian (QWERTY)
nl     // Dutch (QWERTY)
no     // Norwegian (QWERTY/ASERTT)
ph     // Filipino (QWERTY)
pl     // Polish (Programmer, QWERTY/Phonetic)
pl214  // Polish (Typewriter, QWERTZ/Phonetic)
po     // Portuguese (QWERTY)
px     // Portuguese (International, QWERTY)
ro     // Romanian (Standard, QWERTZ/Phonetic)
ro446  // Romanian (QWERTY/Phonetic)
ru     // Russian (Standard, QWERTY/National)
ru443  // Russian (Typewriter, QWERTY/National)
rx     // Russian (Extended Standard, QWERTY/National)
rx443  // Russian (Extended Typewriter, QWERTY/National)
sd     // Swiss (German, QWERTZ)
sf     // Swiss (French, QWERTZ)
si     // Slovenian (QWERTZ)
sk     // Slovak (QWERTZ)
sq     // Albanian (No deadkeys, QWERTY)
sq448  // Albanian (Deadkeys, QWERTZ)
sv     // Swedish (QWERTY/ASERTT)
tj     // Tajik (QWERTY/National)
tm     // Turkmen (QWERTY/Phonetic)
tr     // Turkish (QWERTY)
tr440  // Turkish (Non-Standard)
tt     // Tatar (Standard, QWERTY/National)
tt443  // Tatar (Typewriter, QWERTY/National)
ur     // Ukrainian (101-key, QWERTY/National)
ur1996 // Ukrainian (101-key, 1996, QWERTY/National)
ur2001 // Ukrainian (102-key, 2001, QWERTY/National)
ur2007 // Ukrainian (102-key, 2007, QWERTY/National)
ur465  // Ukrainian (101-key, 465, QWERTY/National)
uz     // Uzbek (QWERTY/National)
vi     // Vietnamese (QWERTY)
yc     // Serbian (Deadkey, QWERTZ/National)
yc450  // Serbian (No deadkey, QWERTZ/National)
yu     // Yugoslavian (QWERTZ)

*/


HostLocale DOS_DetectHostLocale()
{
	HostLocale locale = {};

	// XXX detect the following:
	// XXX locale.messages_language  - GetUserDefaultUILanguage
	// XXX locale.keyboard_layout    - GetKeyboardLayout
	// XXX locale.dos_country        - GetUserDefaultLocaleName
	// XXX locale.numeric
	// XXX locale.time_date
	// XXX locale.currency

	return locale;
}





std::string DOS_GetLayoutFromHost() // XXX this one is outdated, rework it
{
	// Use Windows-specific calls to extract the keyboard layout

	WORD current_kb_layout = LOWORD(GetKeyboardLayout(0));
	WORD current_kb_sub_id = 0;
	char layout_id_string[KL_NAMELENGTH];

	auto parse_hex_string = [](const char* s) {
		uint32_t value = 0;
		sscanf(s, "%x", &value);
		return static_cast<int>(value);
	};

	if (GetKeyboardLayoutName(layout_id_string) &&
	    (safe_strlen(layout_id_string) == 8)) {
		const int current_kb_layout_by_name = parse_hex_string(
		        (char*)&layout_id_string[4]);
		layout_id_string[4] = 0;

		const int sub_id = parse_hex_string((char*)&layout_id_string[0]);

		if ((current_kb_layout_by_name > 0) &&
		    (current_kb_layout_by_name <= UINT16_MAX)) {
			// use layout _id extracted from the layout string
			current_kb_layout = static_cast<WORD>(current_kb_layout_by_name);
		}
		if ((sub_id >= 0) && (sub_id < 100)) {
			// use sublanguage ID extracted from the layout
			// string
			current_kb_sub_id = static_cast<WORD>(sub_id);
		}
	}

	// Try to match emulated keyboard layout with host-keyboardlayout
	switch (current_kb_layout) {
	case 1025:  // Saudi Arabia
	case 1119:  // Tamazight
	case 1120:  // Kashmiri
	case 2049:  // Iraq
	case 3073:  // Egypt
	case 4097:  // Libya
	case 5121:  // Algeria
	case 6145:  // Morocco
	case 7169:  // Tunisia
	case 8193:  // Oman
	case 9217:  // Yemen
	case 10241: // Syria
	case 11265: // Jordan
	case 12289: // Lebanon
	case 13313: // Kuwait
	case 14337: // U.A.E
	case 15361: // Bahrain
	case 16385: // Qatar
		return "ar462";

	case 1026: return "bg";    // Bulgarian
	case 1029: return "cz243"; // Czech
	case 1030: return "dk";    // Danish

	case 2055: // German - Switzerland
	case 3079: // German - Austria
	case 4103: // German - Luxembourg
	case 5127: // German - Liechtenstein
	case 1031: // German - Germany
		return "gr";

	case 1032: return "gk"; // Greek
	case 1034: return "sp"; // Spanish - Spain (Traditional Sort)
	case 1035: return "su"; // Finnish

	case 1036:  // French - France
	case 2060:  // French - Belgium
	case 4108:  // French - Switzerland
	case 5132:  // French - Luxembourg
	case 6156:  // French - Monaco
	case 7180:  // French - West Indies
	case 8204:  // French - Reunion
	case 9228:  // French - Democratic Rep. of Congo
	case 10252: // French - Senegal
	case 11276: // French - Cameroon
	case 12300: // French - Cote d'Ivoire
	case 13324: // French - Mali
	case 14348: // French - Morocco
	case 15372: // French - Haiti
	case 58380: // French - North Africa
		return "fr";

	case 1037: return "il"; // Hebrew
	case 1038: return current_kb_sub_id ? "hu" : "hu208";
	case 1039: return "is161"; // Icelandic

	case 2064: // Italian - Switzerland
	case 1040: // Italian - Italy
		return "it";

	case 3084: return "ca"; // French - Canada
	case 1041: return "jp"; // Japanese

	case 2067: // Dutch - Belgium
	case 1043: // Dutch - Netherlands
		return "nl";

	case 1044: return "no"; // Norwegian (Bokmål)
	case 1045: return "pl"; // Polish
	case 1046: return "br"; // Portuguese - Brazil

	case 2073: // Russian - Moldava
	case 1049: // Russian
		return "ru";

	case 4122: // Croatian (Bosnia/Herzegovina)
	case 1050: // Croatian
		return "hr";

	case 1051: return "sk"; // Slovak
	case 1052: return "sq"; // Albanian - Albania

	case 2077: // Swedish - Finland
	case 1053: // Swedish
		return "sv";

	case 1055: return "tr"; // Turkish
	case 1058: return "ur"; // Ukrainian
	case 1059: return "bl"; // Belarusian
	case 1060: return "si"; // Slovenian
	case 1061: return "et"; // Estonian
	case 1062: return "lv"; // Latvian
	case 1063: return "lt"; // Lithuanian
	case 1064: return "tj"; // Tajik
	case 1066: return "vi"; // Vietnamese
	case 1067: return "hy"; // Armenian - Armenia
	case 1071: return "mk"; // F.Y.R.O. Macedonian
	case 1079: return "ka"; // Georgian
	case 2070: return "po"; // Portuguese - Portugal
	case 2072: return "ro"; // Romanian - Moldava
	case 5146: return "ba"; // Bosnian (Bosnia/Herzegovina)

	case 2058:  // Spanish - Mexico
	case 3082:  // Spanish - Spain (Modern Sort)
	case 4106:  // Spanish - Guatemala
	case 5130:  // Spanish - Costa Rica
	case 6154:  // Spanish - Panama
	case 7178:  // Spanish - Dominican Republic
	case 8202:  // Spanish - Venezuela
	case 9226:  // Spanish - Colombia
	case 10250: // Spanish - Peru
	case 11274: // Spanish - Argentina
	case 12298: // Spanish - Ecuador
	case 13322: // Spanish - Chile
	case 14346: // Spanish - Uruguay
	case 15370: // Spanish - Paraguay
	case 16394: // Spanish - Bolivia
	case 17418: // Spanish - El Salvador
	case 18442: // Spanish - Honduras
	case 19466: // Spanish - Nicaragua
	case 20490: // Spanish - Puerto Rico
	case 21514: // Spanish - United States
	case 58378: // Spanish - Latin America
		return "la";
	}

	return "";
}

#endif
