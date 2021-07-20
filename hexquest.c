/*
 * HexQuest
 *
 * Copyright (C) 2016-2017,2021 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <http://www.gnu.org/licenses/>.
 */

#define PNAME "HexQuest"
#define PDESC "MUCK extensions for HexChat"
#define PVERSION "0.13"
#include "hexchat-plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _WIN32
// yeah I know including a .c is bad
// I can fix this later
#include "fnmatch.c"
#else
#include <fnmatch.h>
#endif
#define MAX_SERVERS 1
#define MAX_ZOMBIES 32
#define MAX_ECHO_CMD 5
#define MAX_HIGHLIGHTS 20
#define HIGHLIGHT_NONE 0
#define HIGHLIGHT_COLOR 1
#define HIGHLIGHT_COLOR_TAB 2
#define HIGHLIGHT_FLASH 3
#define MAX_MESSAGE_LEN 8192

static hexchat_plugin *ph;

static int ServerId[MAX_SERVERS];
static char MuckIdentifier[100] = "#$#mcp version:";
static int MuckIdentifierLen = 15;
static char IdleTimeoutString[512] = "A large crane gently grabs you, pulls you towards a large plastic partition, and drops you into the prize bin.  You roll to someplace out of the muck.";

static char WhisperTo[100]="";                     // Last person you whispered to
static char ZombieCommand[MAX_ZOMBIES][100];       // Command prefixes to use to do something as a zombie instead
static char ZombieName[MAX_ZOMBIES][50];           // Names to watch for that indicate text is for a zombie
static char ZombieTabCode[MAX_ZOMBIES][5];         // Tab code for this zombie
static char EchoCommand[MAX_ECHO_CMD][20];         // When these commands are done, print them
static char HighlightWord[MAX_HIGHLIGHTS][30];     // Words to watch for and highlight
static int HighlightLevelOverride[MAX_HIGHLIGHTS]; // Override HighlightLevel for specific words
static int HighlightColor = 9;                     // Color to highlight text to
static int HighlightLevel = 0;                     // Default amount of notice a highlight should give
static int FlashOnMessage = 0;                     // "gui flash" for all messages on server tab
static int FlashOnZombieMessage = 0;               // "gui flash" for all messages on zombie tabs
static int MeetMeNotifier = 0;                     // "gui flash" and bold when you get MEETME notifications
static int ZombieUsePrint = 0;                     // Use prints instead of privmsg for zombie tabs when possible
static int PageTabs = 1;                           // Use tabs for pages
static int WhisperTabs = 0;                        // Use tabs for whispers - not implemented
static int AutoQuote = 1;                          // Automatically use /quote to send raw text when you type on the server tab
static int IgnoreAway = 1;                         // Ignore /back and /away when you attempt them
static int EatPages = 1;                           // Hide pages because PageTabs is handling them
static int BoldWhisper = 1;                        // Bold any whisper messages you receive
static int FlashWhisper = 1;                       // "gui flash" when you're whispered to
static int MultiPages = 0;                         // Multiple person page support - not implemented
static int CloseQueriesOnIdleKick = 1;             // Close queries when you're kicked for idling out

static char HighlightReason[MAX_MESSAGE_LEN] = "";
static char MessageTransformBuffer[MAX_MESSAGE_LEN+4];

// Mask constants
static const char *PageSayYou = "You page, \"*\" to *.";
static const char *PageSayThem = "* pages, \"*\" to you.";
static const char *PageSayThemMulti = "* pages, \"*\" to * and you.";
static const char *PageActYou = "You page-pose, \"*\" to *.";           // 
static const char *PageActThem = "In a page-pose to you, *";            // Name is first word of action event
static const char *PageActThemMulti = "In a page-pose to * and you, *"; //
static const char *ConnectedAs = "You have connected as *.";
static const char *WhisperYou = "You whisper, \"*\" to *.";
static const char *WhisperThem = "* whispers, \"*\" to you.";

void color3() {
	// Find the server tab and switch to its context temporarily
	hexchat_context *Context = hexchat_get_context(ph);
	int Id;	
	hexchat_get_prefs(ph, "id", NULL, &Id);

	// Find the tab via searching through all tabs
	hexchat_list *list = hexchat_list_get(ph, "channels");
	if(list) {
		while(hexchat_list_next(ph, list))
			if(hexchat_list_int(ph, list, "type")==1 && hexchat_list_int(ph, list, "id")==Id) // Server tab = type 1, and same ID
				if(hexchat_set_context(ph,(hexchat_context *)hexchat_list_str(ph, list, "context")))
					hexchat_commandf(ph, "gui color 3");
		hexchat_list_free(ph, list);
	}
	hexchat_set_context(ph, Context);
}

void hexchat_plugin_get_info(char **name, char **desc, char **version, void **reserved) {
	*name = PNAME;
	*desc = PDESC;
	*version = PVERSION;
}

int hexchat_plugin_deinit() {
	hexchat_print(ph, "HexQuest unloaded");
	return 1;
}

// Tries to read an int from the preferences, or gets a default value
static int GetIntOrDefault(const char *Name, int Default) {
	int Value = hexchat_pluginpref_get_int(ph, Name);
	if(Value != -1)
		return Value;
	return Default;
}

// Checks if the current tab belongs to a MUCK, and returns 1 if true
static int IsMUCK() {
	int Id, i;
	hexchat_get_prefs(ph, "id", NULL, &Id);
	for(i=0; i<MAX_SERVERS; i++)
		if(ServerId[i] == Id)
			return 1;
	return 0;
}

// Checks if the current tab is a query, returns 1 if true
static int IsQuery() {
	return strcmp(hexchat_get_info(ph, "network"), hexchat_get_info(ph, "channel")) && hexchat_get_info(ph, "channel")[0]!='$';
}

// KeyPress event, entirely for the whisper auto-fill thing
static int KeyPress_cb(char *word[], void *userdata) {
	if(IsMUCK() && !*hexchat_get_info(ph, "inputbox") && !IsQuery() && (!strcmp(word[1], "65289"))) {
		hexchat_commandf(ph, "settext wh %s=", WhisperTo);
		hexchat_commandf(ph, "setcursor %i", 12+strlen(WhisperTo));
	}
	return HEXCHAT_EAT_NONE;
}

// Like memcmp but case insensitive
int memcasecmp(const char *Text1, const char *Text2, int Length) {
	for(;Length;Length--)
		if(tolower(*(Text1++)) != tolower(*(Text2++)))
			return 1;
	return 0;
}

// Removes the first word from a string
void RemoveFirstWord(char *String, char *Name) {
	char *Next = strchr(String, ' ');
	if(Next && Next[1]) {
		// You can request to have the first word copied somewhere else
		if(Name) {
			*Next = 0;
			strcpy(Name, String);
		}
		memmove(String, Next+1, strlen(Next+1)+1); // +1 to get the terminator too
	}
}

// Extracts values from a string, given wildcards
void WildExtract(const char *Input, const char *Wild, char **Output, int MaxParams) {
	char Text[strlen(Input)+1];
	char Mask[strlen(Wild)+1];
	strcpy(Text, Input);
	strcpy(Mask, Wild);

	// Clear out the array first
	int CurParam;
	for(CurParam = 0; CurParam < MaxParams; CurParam++)
		Output[CurParam] = NULL;
	CurParam = 0;

	char *End = Text+strlen(Text);
	char *Ptr = strtok(Mask, "*");
	char *Search = Text;
	while(Ptr != NULL) {
		int Len = strlen(Ptr);
		// clear out the thing from the mask
		Search = strstr(Search, Ptr);
		if(!Search)
			break;

		// fix parsing issues
		char *TempSearch;
		while(1) {
			TempSearch = strstr(Search+1, Ptr);
			if(!TempSearch)
				break;
			Search = TempSearch;
		}

		memset(Search, 0, Len);
		Search += Len;

		// find the next thing in the mask
		Ptr = strtok(NULL, "*");
	}

	int Ready = 1;
	for(Ptr = Text; Ptr != End; Ptr++) {
		 if(!*Ptr)
			 Ready = 1;
		 if(*Ptr && Ready) {
			 Output[CurParam++] = strdup(Ptr);
			 if(CurParam >= MaxParams)
				 return;
			 Ready = 0;
		 }
	}
}

// Frees the output from a WildExtract
void WildExtractFree(char **Output, int MaxParams) {
	int i;
	for(i=0; i<MaxParams; i++)
		if(Output[i])
			free(Output[i]);
}

/*
// Replace codes in an input string with other strings
void TextInterpolate(char *Out, const char *In, char Prefix, const char *ReplaceThis, const char *ReplaceWith[]) {
	while(*In) {
		if(*In != Prefix)
			*(Out++) = *(In++);
		else {
			In++;
			char *Find = strchr(ReplaceThis, *(In++));
			if(Find) {
				int This = Find - ReplaceThis;
				strcpy(Out, ReplaceWith[This]);
				Out += strlen(ReplaceWith[This]);
			} else {
				*(Out++) = Prefix;
				*(Out++) = In[-1];
			}
		}
	}
	*Out = 0;
}
*/

// Does a wildcard match
int WildMatch(const char *TestMe, const char *Wild) {
	char NewWild[strlen(Wild)+1];
	char NewTest[strlen(TestMe)+1];
	const char *Asterisk = strchr(Wild, '*');
	if(Asterisk && !Asterisk[1]) // Optimization for the wildcard happening at the end
		return(!memcasecmp(TestMe, Wild, strlen(Wild)-1));

	strcpy(NewTest, TestMe);
	strcpy(NewWild, Wild);
	int i;
	for(i=0;NewWild[i];i++)
		NewWild[i] = tolower(NewWild[i]);
	for(i=0;NewTest[i];i++)
		NewTest[i] = tolower(NewTest[i]);
	return !fnmatch(NewWild, NewTest, FNM_NOESCAPE); // Surely there has to be a better way than using fnmatch for this
}

// Takes a boolean string and converts it to a value
static int TextToBoolean(const char *Text) {
	if(!strcasecmp(Text, "yes") || !strcasecmp(Text, "on"))
		return 1;
	if(!strcasecmp(Text, "no") || !strcasecmp(Text, "off"))
		return 0;
	return -1;
}

// Separates possessives and contractions out from a name
void ContractionFix(char *name, char *output) {
	// Can be an apostrophe followed by one or two characters
	char *apostrophe = strrchr(name, '\'');
	if(!apostrophe || apostrophe == name || (apostrophe[2] != 0 && apostrophe[3] != 0))
		return;
	int len = strlen(apostrophe);
	memmove(output+len+1, output, strlen(output)+1);
	memcpy(output, apostrophe, len);
	output[len] = ' ';
	*apostrophe = 0;
}

// Hack to prevent some problems
static int ZombieIgnore = 0;

// Process pages meant for either the main character or zombies
static int ProcessPages(const char *prefix, const char *your_name, const char *message) {
	char TabName[40];

	char *Output[5]; // Output for WildExtract
	char Name[100];  // Name to extract from some message types
	if(WildMatch(message, PageSayYou)) {
		WildExtract(message, PageSayYou, Output, 2);

		// Switch to the appropriate context, make an event, and switch back
		hexchat_context *Old = hexchat_get_context(ph);

		hexchat_context *Context;
		if(prefix) {
			sprintf(TabName, "%s-%s", prefix, Output[1]);
			Context = hexchat_find_context(ph, NULL, TabName);
		} else {
			Context = hexchat_find_context(ph, NULL, Output[1]);
		}
		if(!Context) {
			WildExtractFree(Output, 2);
			return 0;
		}
		hexchat_set_context(ph, Context);
		hexchat_emit_print(ph, "Your Message", your_name, Output[0], "", "", NULL);
		hexchat_set_context(ph, Old);
		
		WildExtractFree(Output, 2);
		if(EatPages)
			return 1;
	} else if(WildMatch(message, PageActYou)) {
		WildExtract(message, PageActYou, Output, 2);
		RemoveFirstWord(Output[0], Name);

		// Switch to the appropriate context, make an event, and switch back
		hexchat_context *Old = hexchat_get_context(ph);

		hexchat_context *Context;
		if(prefix) {
			sprintf(TabName, "%s-%s", prefix, Output[1]);
			Context = hexchat_find_context(ph, NULL, TabName);
		} else {
			Context = hexchat_find_context(ph, NULL, Output[1]);
		}
		if(!Context) {
			WildExtractFree(Output, 2);
			return 0;
		}
		hexchat_set_context(ph, Context);

		ContractionFix(Name, Output[0]);
		hexchat_emit_print(ph, "Your Action", your_name, Output[0], "", NULL);
		hexchat_set_context(ph, Old);

		WildExtractFree(Output, 2);
		if(EatPages)
			return 1;
	} else if(WildMatch(message, PageSayThem)) {
		WildExtract(message, PageSayThem, Output, 2);
		if(prefix)
			hexchat_commandf(ph, "recv :%s-%s!_@_ PRIVMSG you :%s", prefix, Output[0], Output[1]);
		else
			hexchat_commandf(ph, "recv :%s!_@_ PRIVMSG you :%s", Output[0], Output[1]);
		WildExtractFree(Output, 2);
		if(EatPages)
			return 1;
	} else if(WildMatch(message, PageActThem)) {
		WildExtract(message, PageActThem, Output, 2);
		RemoveFirstWord(Output[0], Name);
		ContractionFix(Name, Output[0]);
		if(prefix)
			hexchat_commandf(ph, "recv :%s-%s!_@_ PRIVMSG you :\x01" "ACTION %s\x01", prefix, Name, Output[0]);
		else
			hexchat_commandf(ph, "recv :%s!_@_ PRIVMSG you :\x01" "ACTION %s\x01", Name, Output[0]);
		WildExtractFree(Output, 2);
		if(EatPages)
			return 1;
	}
	return 0;
}

// Process a message headed for either the server tab or a zombie tab
static int ProcessHighlights(const char *message, int zombie) {
	if(MeetMeNotifier && !memcmp(message, "MEETME: ", 8)) {
		// TODO: let you set up certain people to auto-accept meetme summons from
		hexchat_commandf(ph, "gui flash"); // cause a flash
		sprintf(MessageTransformBuffer, "\x2%s", message); // Bold it
		return 1;
	}
	// Don't process messages that are already processed
	if(message[0] ==2 || strchr(message, 3))
		return 0;

	char *Output[5]; // Output for WildExtract
	if(WildMatch(message, PageSayThemMulti) || WildMatch(message, PageActThemMulti)) {
		sprintf(HighlightReason, "Page: %s", message);
		sprintf(MessageTransformBuffer, "\x2%s", message); // Bold it
		if(!zombie)
			color3();
		hexchat_commandf(ph, "gui flash");
		return 1;
	} else if(WildMatch(message, WhisperYou) && BoldWhisper) {
		WildExtract(message, WhisperYou, Output, 2);
		sprintf(MessageTransformBuffer, "\x2%s", message); // Bold it
		strcpy(WhisperTo, Output[1]);
		WildExtractFree(Output, 2);
		return 1;
	} else if(WildMatch(message, WhisperThem) && BoldWhisper) {
		sprintf(MessageTransformBuffer, "\x2%s", message); // Bold it
		sprintf(HighlightReason, "Whisper: %s", message);
		if(!zombie)
			color3();
		if(FlashWhisper)
			hexchat_commandf(ph, "gui flash");
		return 1;
	}

	// Only do highlights if enabled
	if(HighlightLevel) {
		const char *Input = message;

		int MaxHighlightLevel = -1;
		int HasHighlight = 0;

		// Make a lowercase version of the text we're searching through
		char Lowercase[strlen(Input)+1];
		strcpy(Lowercase, Input);
		for(int i=0; Lowercase[i]; i++)
			Lowercase[i] = tolower(Lowercase[i]);

		// Look for the highlight words
		for(int i=0; i<MAX_HIGHLIGHTS; i++) {
			char *WordFound = strstr(Lowercase, HighlightWord[i]);
			if(HighlightWord[i][0] && WordFound && WordFound != Lowercase) {
				int ThisLevel = HighlightLevelOverride[i];
				// Upgrade the highlight level if needed
				if(ThisLevel < 0)
					ThisLevel = HighlightLevel;
				if(ThisLevel > MaxHighlightLevel)
					MaxHighlightLevel = ThisLevel;

				HasHighlight = 1;
			}
		}

		// If there is a highlight, process it
		if(HasHighlight) {
			if(MaxHighlightLevel >= HIGHLIGHT_COLOR_TAB && !zombie) {
				sprintf(HighlightReason, "Message: %s", Input);
				color3();
			}
			if(MaxHighlightLevel >= HIGHLIGHT_FLASH)
				hexchat_commandf(ph, "gui flash");
		
			char Output[MAX_MESSAGE_LEN];
			int in = 0, out = 0;

			// Color the line
			while(Input[in]) {
				// search for each word
				int Found = 0;
				for(int n=0; n<MAX_HIGHLIGHTS; n++) {
					if(!HighlightWord[n][0])
						continue;
					int WordLen = strlen(HighlightWord[n]);
					if(!memcmp(Lowercase+in, HighlightWord[n], WordLen)) {
						sprintf(Output+out, "\x03%.2i", HighlightColor);
						out += 3;
						memcpy(Output+out, Input+in, WordLen);
						out += WordLen;
						Output[out++] = 15; // disable formatting
						in += WordLen; // skip over the word in the input
						Found = 1;
						break;
					}
				}
				if(!Found)
					Output[out++] = Input[in++];
			}
			Output[out] = 0;
			sprintf(MessageTransformBuffer, "\x2\x2%s", Output);
			return 1;
		}
	}
	return 0; // No transformations
}

// Intercepts raw text from servers
static int RawServer_cb(char *word[], char *word_eol[], void *userdata) {
	if(!memcmp(word_eol[1], MuckIdentifier, MuckIdentifierLen)) {
		hexchat_print(ph, "Identified as a MUCK server\n");
		int Id;
		hexchat_get_prefs(ph, "id", NULL, &Id);
		ServerId[0] = Id;

		char CharacterName[100];
		char Password[100];
		if(hexchat_pluginpref_get_str(ph, "character_name", CharacterName) && hexchat_pluginpref_get_str(ph, "character_pass", Password))
			hexchat_commandf(ph, "quote connect %s %s", CharacterName, Password);
	}

	// If it's the MUCK server, enable HexQuest's features
	if(IsMUCK()) {
		// Don't auto reconnect if the disconnect happened due to inactivity
		if(!strcmp(word_eol[1], IdleTimeoutString)) {
			hexchat_print(ph, "Idle timeout, please reconnect manually");
			hexchat_command(ph, "server 127.0.0.1");
			hexchat_command(ph, "timer 1 quit");
			if(CloseQueriesOnIdleKick)
				hexchat_command(ph, "timer 2 close -m");
			return HEXCHAT_EAT_NONE;
		}

		if(ZombieIgnore) {
			ZombieIgnore = 0;
			return HEXCHAT_EAT_NONE;
		}
		if(word_eol[1][0] == '@') { // Attempt at a fix for someone whose name starts with @
			hexchat_commandf(ph, "recv (at)%s", word_eol[1]+1);
			return HEXCHAT_EAT_ALL;
		}
		if(strstr(word_eol[1], "PRIVMSG")) {
			return HEXCHAT_EAT_NONE;
		}

		// Search the zombie list to see if it's a line meant for a zombie
		for(int i=0; i<MAX_ZOMBIES; i++)
			if(ZombieName[i][0] && !memcmp(word_eol[1], ZombieName[i], strlen(ZombieName[i]))) {
				char *StartOfText = strchr(word_eol[1], '>');
				if(!StartOfText)
					continue;

				// It's a zombie message!
				if(FlashOnZombieMessage) // flash on all zombe messages if you want
					hexchat_commandf(ph, "gui flash");

				// Tab to send this to
				char QueryName[20];
				sprintf(QueryName, "$Z%i", i);
				if(ZombieTabCode[i][0]) {
					sprintf(QueryName, "$Z%s", ZombieTabCode[i]);
				}

				// Remove the > from the name
				char StrippedName[50];
				strcpy(StrippedName, ZombieName[i]);
				char *StripGT = strrchr(StrippedName, '>');
				if(StripGT)
					*StripGT = 0;

				// If it's a page, handle that specially
				if(PageTabs && ProcessPages(QueryName, StrippedName, StartOfText+2))
					return HEXCHAT_EAT_HEXCHAT;

				// Use a print event instead if it's enabled and if the window is open
				if(ZombieUsePrint) {
					hexchat_context *ZombieContext = hexchat_find_context(ph, NULL, QueryName);
					if(ZombieContext) {
						hexchat_context *OldContext = hexchat_get_context(ph);
						hexchat_set_context(ph, ZombieContext);
						if(ProcessHighlights(StartOfText+2, 1)) {
							hexchat_command(ph, "gui color 3"); // Show that there's a notification/highlight
							hexchat_print(ph, MessageTransformBuffer);
						} else {
							hexchat_command(ph, "gui color 2"); // Show that there's a message
							hexchat_print(ph, StartOfText+2);
						}
						hexchat_set_context(ph, OldContext);
						return HEXCHAT_EAT_HEXCHAT;
					}
				}

				ZombieIgnore = 1;
				// Calling ProcessHighlights here won't work with highlighting the tab correctly, but that doesn't matter a lot
				hexchat_commandf(ph, "recv :%s!%s@_ PRIVMSG you :%s", QueryName, StrippedName, ProcessHighlights(StartOfText+2, 1)?MessageTransformBuffer:StartOfText+2);
				return HEXCHAT_EAT_HEXCHAT;
			}

		if(FlashOnMessage) // flash on all messages if you want
			hexchat_commandf(ph, "gui flash");

		// Try parsing the different kinds of message we want special behavior for
		char Name[100];  // Name to extract from some message types
		if(WildMatch(word_eol[1], ConnectedAs)) {
			// Get the name
			char *As = strstr(word_eol[1], "as");
			if(!As)
				return HEXCHAT_EAT_NONE;
			strcpy(Name, As+3);
			As = strrchr(Name, '.');
			if(As)
				*As = 0;

			hexchat_commandf(ph, "recv :%s 001 %s :(changing name in HexChat)", hexchat_get_info(ph, "network"), Name);
		} else if(PageTabs && ProcessPages(NULL, hexchat_get_info(ph, "nick"), word_eol[1])) {
			return HEXCHAT_EAT_ALL;
		} else if(ProcessHighlights(word_eol[1], 0)) {
			hexchat_commandf(ph, "recv %s", MessageTransformBuffer);
			return HEXCHAT_EAT_ALL;
		}
	}
	return HEXCHAT_EAT_NONE;
}

static int AwayBack_cb(char *word[], char *word_eol[], void *userdata) {
	if(IsMUCK() && IgnoreAway)
		return HEXCHAT_EAT_HEXCHAT;
	return HEXCHAT_EAT_NONE;
}

// Find the name for a target, based on the tab name
static const char *TabName() {
	static char Name[64];
	const char *Tab = hexchat_get_info(ph, "channel");
	if(!Tab)
		return "";
	// not a zombie? return the name directly
	if(!strstr(Tab, "(Z)"))
		return Tab;

	strcpy(Name, Tab);
	char *Paren = strchr(Name, '(');
	if(Paren)
		*Paren = 0;
	return Name;
}

static const char *GetZombiePrefix() {
	static char prefix_for_query[100];
	int ZombieNum = -1;

	const char *Tab = hexchat_get_info(ph, "channel");
	const char *Dash = strchr(Tab, '-');
	if(Tab[0] != '$' || Tab[1] != 'Z') // not a zombie
		return "";
	if(isdigit(Tab[2])) {
		ZombieNum = strtol(&Tab[2], NULL, 10);
	} else { // Find the tab code
		for(int i=0; i<MAX_ZOMBIES; i++) {
			if(strcmp(ZombieTabCode[i], Tab+2)) {
				ZombieNum = i;
				break;
			}
		}
	}

	if(ZombieNum < 0 || ZombieNum >= MAX_ZOMBIES) // Bad code
		return "";

	if(Dash) {
		sprintf(prefix_for_query, "%sp %s=", ZombieCommand[ZombieNum], Dash+1);

		// Also remove the zombie suffix here too
		if(strstr(prefix_for_query, "(Z)")) {
			char *Paren = strrchr(prefix_for_query, '(');
			if(Paren) {
				Paren[0] = '=';
				Paren[1] = 0;
			}
		}

		return prefix_for_query;
	} else {
		return ZombieCommand[ZombieNum];
	}
}

static int TrapSay_cb(char *word[], char *word_eol[], void *userdata) {
	if(IsMUCK() && AutoQuote) {
		// Display echo'd commands on the log
		for(int i=0; i<MAX_ECHO_CMD; i++)
			if(EchoCommand[i][0] && !memcmp(word_eol[1], EchoCommand[i], strlen(EchoCommand[i]))) {
				hexchat_printf(ph, "%s", word_eol[1]);
				break;
			}

		const char *Prefix = GetZombiePrefix();
		if(*Prefix) {
			hexchat_commandf(ph, "quote %s%s", Prefix, word_eol[1]);
		} else if(!IsQuery())
			hexchat_commandf(ph, "quote %s", word_eol[1]);
		else
			hexchat_commandf(ph, "quote page %s=%s", TabName(), word_eol[1]);
		return HEXCHAT_EAT_HEXCHAT;
	}
	return HEXCHAT_EAT_NONE;
}

static int TrapAction_cb(char *word[], char *word_eol[], void *userdata) {
	if(IsMUCK() && AutoQuote) {
		const char *Prefix = GetZombiePrefix();
		if(*Prefix) {
			hexchat_commandf(ph, "quote %s:%s", Prefix, word_eol[2]);
		} else if(!IsQuery())
			hexchat_commandf(ph, "quote :%s", word_eol[2]);
		else
			hexchat_commandf(ph, "quote page %s=:%s", TabName(), word_eol[2]);
		return HEXCHAT_EAT_HEXCHAT;
	}
	return HEXCHAT_EAT_NONE;
}

static int Settings_cb(char *word[], char *word_eol[], void *userdata) {
	const char *LevelNames[] = {"None", "Colors only", "Colors+Tab color", "Colors+Tab color+Flash"};

	if(!strcmp(word[2], "account")) {
		hexchat_pluginpref_set_str(ph, "character_name", word[3]);
		hexchat_pluginpref_set_str(ph, "character_pass", word[4]);
		hexchat_print(ph, "Character name and password changed");
	} else if(!strcmp(word[2], "highlight_word")) {
		int LevelOverride = -1;
		const char *WordNumPtr = word[3];
		if(*WordNumPtr == 'c') {
			LevelOverride = HIGHLIGHT_COLOR;
			WordNumPtr++;
		} else if(*WordNumPtr == 't') {
			LevelOverride = HIGHLIGHT_COLOR_TAB;
			WordNumPtr++;
		} else if(*WordNumPtr == 'f') {
			LevelOverride = HIGHLIGHT_FLASH;
			WordNumPtr++;
		}

		int WordNum = strtol(WordNumPtr, NULL, 10);
		if(WordNum < 0 || WordNum >= MAX_HIGHLIGHTS) {
			hexchat_printf(ph, "Invalid highlight number, use 0 to %i\n", 0, MAX_HIGHLIGHTS-1);
			return HEXCHAT_EAT_ALL;
		}
		if(strlen(word_eol[4]) < 30) {
			char Temp[50];
			strcpy(Temp, word_eol[4]);
			for(int i=0; Temp[i]; i++)
				Temp[i] = tolower(Temp[i]);
			strcpy(HighlightWord[WordNum], Temp);
			sprintf(Temp, "highlight_word_%i", WordNum);
			hexchat_pluginpref_set_str(ph, Temp, HighlightWord[WordNum]);
			sprintf(Temp, "highlight_level_%i", WordNum);
			hexchat_pluginpref_set_int(ph, Temp, LevelOverride);
			hexchat_printf(ph, "Will give a highlight for \"%s\" when it's seen (slot %i)\n", word_eol[4], WordNum);
			if(LevelOverride != -1) {
				hexchat_printf(ph, "Also, highlight level override set to \"%s\"\n", LevelNames[LevelOverride]);
			}
		}
	} else if(!strcmp(word[2], "echo_cmd")) {
		int CommandNum = strtol(word[3], NULL, 10);
		if(CommandNum < 0 || CommandNum >= MAX_ECHO_CMD) {
			hexchat_printf(ph, "Invalid command number, use 0 to %i\n", 0, MAX_ECHO_CMD-1);
			return HEXCHAT_EAT_ALL;
		}
		if(strlen(word[4]) < 20) {
			strcpy(EchoCommand[CommandNum], word[4]);
			char Temp[50];
			sprintf(Temp, "echo_cmd_%i", CommandNum);
			hexchat_pluginpref_set_str(ph, Temp, EchoCommand[CommandNum]);
			hexchat_printf(ph, "Will echo the command \"%s\" when it's used (slot %i)\n", word[4], CommandNum);
		}
	} else if(!strcmp(word[2], "delzombie")) {
		int ZombieNum = strtol(word[3], NULL, 10);
		if(ZombieNum < 0 || ZombieNum >= MAX_ZOMBIES) {
			hexchat_printf(ph, "Invalid zombie number, use 0 to %i\n", 0, MAX_ZOMBIES-1);
			return HEXCHAT_EAT_ALL;
		}
		ZombieCommand[ZombieNum][0] = 0;
		ZombieName[ZombieNum][0] = 0;
		ZombieTabCode[ZombieNum][0] = 0;
		char Temp[50];
		sprintf(Temp, "zombie_name_%i", ZombieNum);
		hexchat_pluginpref_set_str(ph, Temp, "");
		sprintf(Temp, "zombie_action_%i", ZombieNum);
		hexchat_pluginpref_set_str(ph, Temp, "");
		sprintf(Temp, "zombie_tabcode_%i", ZombieNum);
		hexchat_pluginpref_set_str(ph, Temp, "");
	} else if(!strcmp(word[2], "zombie")) {
		int ZombieNum = strtol(word[3], NULL, 10);
		if(ZombieNum < 0 || ZombieNum >= MAX_ZOMBIES) {
			hexchat_printf(ph, "Invalid zombie number, use 0 to %i\n", 0, MAX_ZOMBIES-1);
			return HEXCHAT_EAT_ALL;
		}
		sprintf(ZombieCommand[ZombieNum], "%s ", word[4]);
		sprintf(ZombieName[ZombieNum], "%s> ", word[5]);
		char Temp[50];
		sprintf(Temp, "zombie_name_%i", ZombieNum);
		hexchat_pluginpref_set_str(ph, Temp, ZombieName[ZombieNum]);
		sprintf(Temp, "zombie_action_%i", ZombieNum);
		hexchat_pluginpref_set_str(ph, Temp, ZombieCommand[ZombieNum]);

		hexchat_printf(ph, "Zombie number %i set to action %s, name %s\n", ZombieNum, word[4], word[5]);
	} else if(!strcmp(word[2], "zombietab")) {
		int ZombieNum = strtol(word[3], NULL, 10);
		if(ZombieNum < 0 || ZombieNum >= MAX_ZOMBIES) {
			hexchat_printf(ph, "Invalid zombie number, use 0 to %i\n", 0, MAX_ZOMBIES-1);
			return HEXCHAT_EAT_ALL;
		}
		strcpy(ZombieTabCode[ZombieNum], word[4]);
		char Temp[50];
		sprintf(Temp, "zombie_tabcode_%i", ZombieNum);
		hexchat_pluginpref_set_str(ph, Temp, ZombieTabCode[ZombieNum]);
	} else if(!strcmp(word[2], "zombielist")) {
		hexchat_print(ph, "--All zombies you have:--\n");
		for(int i=0; i<MAX_ZOMBIES; i++) {
			if(ZombieCommand[i][0]) {
				if(ZombieTabCode[i][0]) {
					hexchat_printf(ph, "%d: Name:%sCommand:%s Tab:$Z%s\n", i, ZombieName[i], ZombieCommand[i], ZombieTabCode[i]);
				} else {
					hexchat_printf(ph, "%d: Name:%sCommand:%s Tab:$Z%d\n", i, ZombieName[i], ZombieCommand[i], i);
				}
			}
		}
		hexchat_print(ph, "-------------------------\n");
	} else if(!strcmp(word[2], "force")) {
		hexchat_print(ph, "MUCK mode forced on\n");
		hexchat_get_prefs(ph, "id", NULL, &ServerId[0]);
	} else if(!strcmp(word[2], "page_tabs") || !strcmp(word[2], "whisper_tabs") ||
						!strcmp(word[2], "auto_quote") || !strcmp(word[2], "ignore_away") ||
						!strcmp(word[2], "eat_pages") || !strcmp(word[2], "bold_whisper") ||
						!strcmp(word[2], "flash_whisper") || !strcmp(word[2], "multi_pages") ||
						!strcmp(word[2], "server_flash") || !strcmp(word[2], "meetme_notifier") ||
						!strcmp(word[2], "zombie_print_events") || !strcmp(word[2], "zombie_flash") ||
						!strcmp(word[2], "close_queries_on_idle_kick")) {
		int NewValue = TextToBoolean(word[3]);
		if(NewValue == -1)
			hexchat_print(ph, "Invalid value (use on/off)\n");
		else {
			hexchat_printf(ph, "\"%s\" %s\n", word[2], NewValue?"enabled":"disabled");
			hexchat_pluginpref_set_int(ph, word[2], NewValue);

			// Update the variables in RAM			
			if(!strcmp(word[2], "page_tabs"))
				PageTabs = NewValue;
			else if(!strcmp(word[2], "whisper_tabs"))
				WhisperTabs = NewValue;
			else if(!strcmp(word[2], "auto_quote"))
				AutoQuote = NewValue;
			else if(!strcmp(word[2], "ignore_away"))
				IgnoreAway = NewValue;
			else if(!strcmp(word[2], "eat_pages"))
				EatPages = NewValue;
			else if(!strcmp(word[2], "bold_whisper"))
				BoldWhisper = NewValue;
			else if(!strcmp(word[2], "flash_whisper"))
				FlashWhisper = NewValue;
			else if(!strcmp(word[2], "server_flash"))
				FlashOnMessage = NewValue;
			else if(!strcmp(word[2], "zombie_flash"))
				FlashOnZombieMessage = NewValue;
			else if(!strcmp(word[2], "meetme_notifier"))
				MeetMeNotifier = NewValue;
			else if(!strcmp(word[2], "zombie_print_events"))
				ZombieUsePrint = NewValue;
			else if(!strcmp(word[2], "close_queries_on_idle_kick"))
				CloseQueriesOnIdleKick = NewValue;
		}
	} else if(!strcmp(word[2], "muck_identifier") || !strcmp(word[2], "idle_timeout_string")) {
		hexchat_pluginpref_set_str(ph, word[2], word_eol[3]);
		hexchat_printf(ph, "\"%s\" set to \"%s\"\n", word[2], word_eol[3]);

		// Update the variables in RAM
		if(!strcmp(word[2], "muck_identifier")) {
			strcpy(MuckIdentifier, word_eol[3]);
			MuckIdentifierLen = strlen(MuckIdentifier);
		} else if(!strcmp(word[2], "idle_timeout_string")) {
			strcpy(IdleTimeoutString, word_eol[3]);
		}
	} else if(!strcmp(word[2], "highlight_level")) {
		HighlightLevel = strtol(word[3], NULL, 10) & 3;
		hexchat_printf(ph, "Highlight level set to %i (%s)", HighlightLevel, LevelNames[HighlightLevel]);
	} else if(!strcmp(word[2], "highlight_color")) {
		HighlightColor = strtol(word[3], NULL, 10);
		hexchat_printf(ph, "\x03%i Highlight color changed", HighlightColor);
	} else if(!strcmp(word[2], "highlight_reason")) {
		hexchat_printf(ph, "Highlight reason: %s", HighlightReason);
	}
	return HEXCHAT_EAT_HEXCHAT;
}

int hexchat_plugin_init(hexchat_plugin *plugin_handle,
											char **plugin_name,
											char **plugin_desc,
											char **plugin_version,
											char *arg) {
	ph = plugin_handle;
	*plugin_name = PNAME;
	*plugin_desc = PDESC;
	*plugin_version = PVERSION;
	hexchat_print(ph, "HexQuest loaded");
	memset(ServerId, -1, sizeof(ServerId)); // due to two's complement this will get set to negative 1
	memset(ZombieCommand, 0, sizeof(ZombieCommand));
	memset(ZombieName, 0, sizeof(ZombieName));
	memset(ZombieTabCode, 0, sizeof(ZombieTabCode));
	memset(HighlightWord, 0, sizeof(HighlightWord));
	memset(EchoCommand, 0, sizeof(EchoCommand));

	PageTabs = GetIntOrDefault("page_tabs", 1);
	WhisperTabs = GetIntOrDefault("whisper_tabs", 0);
	AutoQuote = GetIntOrDefault("auto_quote", 1);
	IgnoreAway = GetIntOrDefault("ignore_away", 1);
	EatPages = GetIntOrDefault("eat_pages", 1);
	BoldWhisper = GetIntOrDefault("bold_whisper", 1);
	FlashWhisper = GetIntOrDefault("flash_whisper", 1);
	MultiPages = GetIntOrDefault("multi_pages", 1);
	MeetMeNotifier = GetIntOrDefault("meetme_notifier", 1);
	HighlightColor = GetIntOrDefault("highlight_color", 9);
	HighlightLevel = GetIntOrDefault("highlight_level", HIGHLIGHT_COLOR);
	ZombieUsePrint = GetIntOrDefault("zombie_print_events", 1);
	CloseQueriesOnIdleKick = GetIntOrDefault("close_queries_on_idle_kick", 1);

	hexchat_pluginpref_get_str(ph, "idle_timeout_string", IdleTimeoutString);
	hexchat_pluginpref_get_str(ph, "muck_identifier", MuckIdentifier);
	MuckIdentifierLen = strlen(MuckIdentifier);

	char Temp[50];
	for(int i=0; i<MAX_ZOMBIES; i++) {
		sprintf(Temp, "zombie_name_%i", i);
		hexchat_pluginpref_get_str(ph, Temp, ZombieName[i]);
		sprintf(Temp, "zombie_action_%i", i);
		hexchat_pluginpref_get_str(ph, Temp, ZombieCommand[i]);
		sprintf(Temp, "zombie_tabcode_%i", i);
		hexchat_pluginpref_get_str(ph, Temp, ZombieTabCode[i]);
	}
	for(int i=0; i<MAX_ECHO_CMD; i++) {
		sprintf(Temp, "echo_cmd_%i", i);
		hexchat_pluginpref_get_str(ph, Temp, EchoCommand[i]);
	}
	for(int i=0; i<MAX_HIGHLIGHTS; i++) {
		sprintf(Temp, "highlight_word_%i", i);
		hexchat_pluginpref_get_str(ph, Temp, HighlightWord[i]);
		sprintf(Temp, "highlight_level_%i", i);
		HighlightLevelOverride[i] = GetIntOrDefault(Temp, -1);
	}

	hexchat_hook_command(ph, "", HEXCHAT_PRI_NORM, TrapSay_cb, NULL, 0);
	hexchat_hook_command(ph, "me", HEXCHAT_PRI_NORM, TrapAction_cb, NULL, 0);
	hexchat_hook_command(ph, "hquest", HEXCHAT_PRI_NORM, Settings_cb, NULL, 0); // maybe put a note about where to find help later
	hexchat_hook_command(ph, "away", HEXCHAT_PRI_NORM, AwayBack_cb, NULL, 0);
	hexchat_hook_command(ph, "back", HEXCHAT_PRI_NORM, AwayBack_cb, NULL, 0);
	hexchat_hook_server(ph, "RAW LINE", HEXCHAT_PRI_NORM, RawServer_cb, NULL);
	hexchat_hook_print(ph, "Key Press", HEXCHAT_PRI_NORM, KeyPress_cb, NULL); 
	return 1;
}
