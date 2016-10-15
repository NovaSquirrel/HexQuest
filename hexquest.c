/*
 * HexQuest
 *
 * Copyright (C) 2016 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define PNAME "HexQuest"
#define PDESC "MUCK extensions for HexChat"
#define PVERSION "0.02"
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
static hexchat_plugin *ph;

static int ServerID = -1;
static char MuckIdentifier[100] = "#$#mcp version:";
static int MuckIdentifierLen = 15;
static char IdleTimeoutString[512] = "A large crane gently grabs you, pulls you towards a large plastic partition, and drops you into the prize bin.  You roll to someplace out of the muck.";
static char WhisperTo[100]="";

// Mask constants
static const char *PageSayYou = "You page, \"*\" to *.";
static const char *PageSayThem = "* pages, \"*\" to you.";
static const char *PageActYou = "You page-pose, \"*\" to *."; // name is first word of action
static const char *PageActThem = "In a page-pose to you, *";  // name is first word of action
static const char *ConnectedAs = "You have connected as *.";
static const char *WhisperYou = "You whisper, \"*\" to *.";
static const char *WhisperThem = "* whispers, \"*\" to you.";

// Feature toggles
static int PageTabs, WhisperTabs, AutoQuote, IgnoreAway, EatPages, BoldWhisper;

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
  int Id;  
  hexchat_get_prefs(ph, "id", NULL, &Id);
  return Id == ServerID;
}

// Checks if the current tab is a query, returns 1 if true
static int IsQuery() {
  return strcmp(hexchat_get_info(ph, "network"), hexchat_get_info(ph, "channel"));
}

static int KeyPress_cb(char *word[], void *userdata) {
  if(!strlen(hexchat_get_info(ph, "inputbox")) && IsMUCK() && !IsQuery() && (!strcmp(word[1], "65289") || !strcmp(word[3], "\t"))) {
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

// Does a wildcard match
int WildMatch(const char *TestMe, const char *Wild) {
  char NewWild[strlen(Wild)+1];
  char NewTest[strlen(TestMe)+1];
  const char *Asterisk = strchr(Wild, '*');
  if(Asterisk && !Asterisk[1]) return(!memcasecmp(TestMe, Wild, strlen(Wild)-1));

  strcpy(NewTest, TestMe);
  strcpy(NewWild, Wild);
  int i;
  for(i=0;NewWild[i];i++)
    NewWild[i] = tolower(NewWild[i]);
  for(i=0;NewTest[i];i++)
    NewTest[i] = tolower(NewTest[i]);
  return !fnmatch(NewWild, NewTest, FNM_NOESCAPE);
}

// Takes a boolean string and converts it to a value
static int TextToBoolean(const char *Text) {
  if(!strcmp(Text, "yes") || !strcmp(Text, "on"))
    return 1;
  if(!strcmp(Text, "no") || !strcmp(Text, "off"))
    return 0;
  return -1;
}

static int RawServer_cb(char *word[], char *word_eol[], void *userdata) {
  if(!memcmp(word_eol[1], MuckIdentifier, MuckIdentifierLen)) {
    hexchat_print(ph, "Identified as a MUCK server\n");
    hexchat_get_prefs(ph, "id", NULL, &ServerID);

    char CharacterName[100];
    char Password[100];
    if(hexchat_pluginpref_get_str(ph, "character_name", CharacterName) && hexchat_pluginpref_get_str(ph, "character_pass", Password))
      hexchat_commandf(ph, "timer 2 quote connect %s %s", CharacterName, Password);
  }

  if(IsMUCK()) {
    // Don't auto reconnect if the disconnect happened due to inactivity
    char *Output[5];
    char Name[100];
    if(!strcmp(word_eol[1], IdleTimeoutString)) {
      hexchat_print(ph, "Idle timeout, please reconnect manually");
      hexchat_command(ph, "server 127.0.0.1");
      hexchat_command(ph, "timer 1 quit");
    } else if(word_eol[1][0]!=2 && (WildMatch(word_eol[1], WhisperYou))) {
      WildExtract(word_eol[1], WhisperYou, Output, 2);
      hexchat_commandf(ph, "recv \x02%s", word_eol[1]);
      strcpy(WhisperTo, Output[1]);
      WildExtractFree(Output, 2);
      return HEXCHAT_EAT_HEXCHAT;
    } else if(word_eol[1][0]!=2 && (WildMatch(word_eol[1], WhisperThem))) {
      hexchat_commandf(ph, "recv \x02%s", word_eol[1]);
      hexchat_commandf(ph, "gui color 3");
      return HEXCHAT_EAT_HEXCHAT;
    } else if(WildMatch(word_eol[1], ConnectedAs)) {
      // get the name
      char *As = strstr(word_eol[1], "as");
      if(!As)
        return HEXCHAT_EAT_NONE;
      strcpy(Name, As+3);
      As = strrchr(Name, '.');
      if(As)
        *As = 0;

      hexchat_commandf(ph, "recv :%s 001 %s :(changing name in HexChat)", hexchat_get_info(ph, "network"), Name);
    } else if(WildMatch(word_eol[1], PageSayYou)) {
      WildExtract(word_eol[1], PageSayYou, Output, 2);

      // Switch to the appropriate context, make an event, and switch back
      hexchat_context *Old = hexchat_get_context(ph);
      hexchat_context *Context = hexchat_find_context(ph, NULL, Output[1]);
      if(!Context)
        return HEXCHAT_EAT_NONE;
      hexchat_set_context(ph, Context);
      hexchat_emit_print(ph, "Your Message", hexchat_get_info(ph, "nick"), Output[0], "", "", NULL);
      hexchat_set_context(ph, Old);
      
      WildExtractFree(Output, 2);
      if(EatPages)
        return HEXCHAT_EAT_HEXCHAT;
    } else if(WildMatch(word_eol[1], PageSayThem)) {
      WildExtract(word_eol[1], PageSayThem, Output, 2);
      hexchat_commandf(ph, "recv :%s!_@_ PRIVMSG you :%s", Output[0], Output[1]);
      WildExtractFree(Output, 2);
      if(EatPages)
        return HEXCHAT_EAT_HEXCHAT;
    } else if(WildMatch(word_eol[1], PageActYou)) {
      WildExtract(word_eol[1], PageActYou, Output, 2);
      RemoveFirstWord(Output[0], NULL);

      // Switch to the appropriate context, make an event, and switch back
      hexchat_context *Old = hexchat_get_context(ph);
      hexchat_context *Context = hexchat_find_context(ph, NULL, Output[1]);
      if(!Context)
        return HEXCHAT_EAT_NONE;
      hexchat_set_context(ph, Context);
      hexchat_emit_print(ph, "Your Action", hexchat_get_info(ph, "nick"), Output[0], "", NULL);
      hexchat_set_context(ph, Old);

      WildExtractFree(Output, 2);
      if(EatPages)
        return HEXCHAT_EAT_HEXCHAT;
    } else if(WildMatch(word_eol[1], PageActThem)) {
      WildExtract(word_eol[1], PageActThem, Output, 2);
      RemoveFirstWord(Output[0], Name);
      hexchat_commandf(ph, "recv :%s!_@_ PRIVMSG you :\x01" "ACTION %s\x01", Name, Output[0]);
      WildExtractFree(Output, 2);
      if(EatPages)
        return HEXCHAT_EAT_HEXCHAT;
    }
  }
  return HEXCHAT_EAT_NONE;
}

static int AwayBack_cb(char *word[], char *word_eol[], void *userdata) {
  if(IsMUCK() && IgnoreAway)
    return HEXCHAT_EAT_HEXCHAT;
  return HEXCHAT_EAT_NONE;
}

static int TrapSay_cb(char *word[], char *word_eol[], void *userdata) {
  if(IsMUCK() && AutoQuote) {
    if(!IsQuery())
      hexchat_commandf(ph, "quote %s", word_eol[1]);
    else
      hexchat_commandf(ph, "quote page %s=%s", hexchat_get_info(ph, "channel"), word_eol[1]);
    return HEXCHAT_EAT_HEXCHAT;
  }
  return HEXCHAT_EAT_NONE;
}

static int TrapAction_cb(char *word[], char *word_eol[], void *userdata) {
  if(IsMUCK() && AutoQuote) {
    if(!IsQuery())
      hexchat_commandf(ph, "quote :%s", word_eol[2]);
    else
      hexchat_commandf(ph, "quote page %s=:%s", hexchat_get_info(ph, "channel"), word_eol[2]);
    return HEXCHAT_EAT_HEXCHAT;
  }
  return HEXCHAT_EAT_NONE;
}

static int Settings_cb(char *word[], char *word_eol[], void *userdata) {

  if(!strcmp(word[2], "account")) {
    hexchat_pluginpref_set_str(ph, "character_name", word[3]);
    hexchat_pluginpref_set_str(ph, "character_pass", word[4]);
    hexchat_print(ph, "Character name and password changed");
  } else if(!strcmp(word[2], "force")) {
    hexchat_print(ph, "MUCK mode forced on\n");
    hexchat_get_prefs(ph, "id", NULL, &ServerID);
  } else if(!strcmp(word[2], "page_tabs") || !strcmp(word[2], "whisper_tabs") ||
            !strcmp(word[2], "auto_quote") || !strcmp(word[2], "ignore_away") ||
            !strcmp(word[2], "eat_pages") || !strcmp(word[2], "bold_whisper")) {
    int NewValue = TextToBoolean(word[2]);
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

  PageTabs = GetIntOrDefault("page_tabs", 1);
  WhisperTabs = GetIntOrDefault("whisper_tabs", 0);
  AutoQuote = GetIntOrDefault("auto_quote", 1);
  IgnoreAway = GetIntOrDefault("ignore_away", 1);
  EatPages = GetIntOrDefault("eat_pages", 1);
  BoldWhisper = GetIntOrDefault("bold_whisper", 1);

  hexchat_pluginpref_get_str(ph, "idle_timeout_string", IdleTimeoutString);
  hexchat_pluginpref_get_str(ph, "muck_identifier", MuckIdentifier);
  MuckIdentifierLen = strlen(MuckIdentifier);

  hexchat_hook_command(ph, "", HEXCHAT_PRI_NORM, TrapSay_cb, NULL, 0);
  hexchat_hook_command(ph, "me", HEXCHAT_PRI_NORM, TrapAction_cb, NULL, 0);
  hexchat_hook_command(ph, "hquest", HEXCHAT_PRI_NORM, Settings_cb, NULL, 0); // maybe put a note about where to find help later
  hexchat_hook_command(ph, "away", HEXCHAT_PRI_NORM, AwayBack_cb, NULL, 0);
  hexchat_hook_command(ph, "back", HEXCHAT_PRI_NORM, AwayBack_cb, NULL, 0);
  hexchat_hook_server(ph, "RAW LINE", HEXCHAT_PRI_NORM, RawServer_cb, NULL);
  hexchat_hook_print(ph, "Key Press", HEXCHAT_PRI_NORM, KeyPress_cb, NULL); 
  return 1;
}

