/*
	Copyright (C) 2004 Cory Nelson

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the
	"Software"), to deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to
	permit persons to whom the Software is furnished to do so, subject to
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "conf.h"

CONF *ConfCreate(void) {
	return (CONF*)calloc(1, sizeof(CONF));
}

void ConfDestroy(CONF *conf) {
	CONFNODE *c, *next;
	for(c=conf->first; c!=NULL; c=next) {
		if(c->key) free(c->key);
		if(c->value) free(c->value);
		
		next=c->next;
		free(c);
	}
	free(conf);
}

LPCTSTR ConfGetOpt(const CONF *conf, LPCTSTR key) {
	const CONFNODE *c;
	for(c=conf->first; c!=NULL; c=c->next)
		if(!_tcscmp(c->key, key)) return c->value;
	return NULL;
}

static LPTSTR tcsdup_trim(LPCTSTR str) {
	const TCHAR *start, *end;
	LPTSTR newstr;
	size_t newlen;

	start=str;
	while(_istspace(*start)) start++;

	end=start+_tcslen(start)-1;
	while(end>start && _istspace(*end)) end--;

	newlen=end-start+1;
	newstr=malloc((newlen+1)*sizeof(TCHAR));
	memcpy(newstr, start, newlen*sizeof(TCHAR));
	newstr[newlen]='\0';

	return newstr;
}

void ConfSetOpt(CONF *conf, LPCTSTR key, LPCTSTR value) {
	CONFNODE *c;
	for(c=conf->first; c!=NULL; c=c->next) {
		if(!_tcscmp(c->key, key)) {
			if(c->value) free(c->value);
			c->value=_tcsdup(value);
			return;
		}
	}

	c=malloc(sizeof(CONFNODE));
	c->key=tcsdup_trim(key);
	c->value=tcsdup_trim(value);
	c->next=NULL;

	if(conf->last) conf->last=conf->last->next=c;
	else conf->last=conf->first=c;
}

BOOL ConfLoad(CONF *conf, LPCTSTR file) {
	TCHAR buf[512];
	FILE *fp=_tfopen(file, _T("r"));
	if(!fp) return FALSE;

	while(_fgetts(buf, 512, fp)) {
		TCHAR key[512], value[512];
		if(_stscanf(buf, _T("%[^=]=%[^\r\n]"), key, value)==2)
			ConfSetOpt(conf, key, value);
	}

	fclose(fp);

	return TRUE;
}

BOOL ConfSave(const CONF *conf, LPCTSTR file) {
	const CONFNODE *c;
	FILE *fp=_tfopen(file, _T("w"));
	if(!fp) return FALSE;

	for(c=conf->first; c!=NULL; c=c->next)
		_ftprintf(fp, _T("%s=%s\n"), c->key, c->value);

	fclose(fp);

	return TRUE;
}
