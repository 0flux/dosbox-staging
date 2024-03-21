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

// XXX use:
// - CFLocaleCopyCurrent / CFLocaleGetValue
// - kCFLocaleCountryCode / kCFLocaleLanguageCode

// XXX see: https://keyshorts.com/blogs/blog/37615873-how-to-identify-macbook-keyboard-localization

/* XXX
	#include <CoreFoundation/CoreFoundation.h>

	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	auto value = (CFStringRef)CFLocaleGetValue(cflocale, kCFLocaleLanguageCode);    
	std::string str(CFStringGetCStringPtr(value, kCFStringEncodingASCII));
	CFRelease(cflocale);


	char layout[128];
	TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    	CFStringRef layoutID = TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    	CFStringGetCString(layoutID, layout, sizeof(layout), kCFStringEncodingASCII);
    	printf("%s\n", layout);

   // keyboard layouts:

    US English
    US English International
    UK (British) English
    Arabic
    Armenian
    Azeri/Azerbaijani
    Belgian
    Bengali
    Bosnian
    Bulgarian
    Burmese
    Cherokee
    Chinese
    Colemak
    Croatian
    Czech
    Danish
    Dvorak
    Dutch
    Estonian
    Finnish
    French
    French (Canadian)
    Georgian
    German
    Greek
    Greek (Polytonic)
    Gujarati
    Hebrew
    Hindi
    Hungarian
    Icelandic
    Inuktitut
    Italian
    Japanese
    Kannada
    Kazakh
    Khmer
    Korean
    Kurdish (Sorani)
    Latvian
    Lithuanian
    Macedonian
    Malay (Jawi)
    Malayalam
    Maltese
    Nepali
    Northern Sami
    Norwegian
    Odia/Oriya
    Pashto
    Persian/Farsi
    Polish
    Polish Pro
    Portuguese
    Portuguese (Brazilian)
    Punjabi (Gurmukhi)
    Romanian
    Russian
    Russian (Phonetic)
    Serbian
    Serbian (Latin)
    Sinhala
    Slovak
    Slovene/Slovenian
    Spanish
    Spanish (Latin America)
    Swedish
    Swiss
    Tamil
    Telugu
    Thai
    Tibetan
    Turkish F
    Turkish Q
    Ukrainian
    Urdu
    Uyghur
    Uzbek
    Vietnamese





*/