#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <getopt.h>

#define COLOR  L"\033[047m\x1B[31m"
#define RESET L"\033[0m\x1B[0m"
#define CLRSCR L"\033[H\033[J"

struct list_node
{
	wchar_t *str;
	struct list_node *prev;
	struct list_node *next;
};

typedef struct
{
	wchar_t *s1;
	wchar_t *s2;
}Twochar_w;

typedef struct
{
	int x, y, terminaltop, terminalbottom, terminalleft, terminalright, mode;
	wchar_t *prevsearch;
	size_t num_elements;
	struct winsize window;
	struct list_node *curr;
	struct list_node *head;
	struct list_node *tail;
} Biderect_list;

Biderect_list text;

struct list_node* GetNewNode(wchar_t *x) {
	struct list_node* newNode
		= (struct list_node*)malloc(sizeof(struct list_node));
	text.num_elements++;
	newNode->str = x;
	newNode->prev = NULL;
	newNode->next = NULL;
	return newNode;
}


void InsertAtHead(wchar_t *x) {
	struct list_node* newNode = GetNewNode(x);
	if(text.head == NULL) {
		text.head = newNode;
			if(text.tail == NULL)
		{
			text.tail = newNode;
		}
		return;
	}

	text.head->prev = newNode;
	newNode->next = text.head; 
	text.head = newNode;
}

void InsertAtTail(wchar_t *x) {
	struct list_node* temp = text.tail;
	struct list_node* newNode = GetNewNode(x);
	if(text.head == NULL) {		
		text.head = newNode;
			if(text.tail == NULL)
		{
			text.tail = newNode;
		}
		return;
	}
	temp = text.tail;
	temp->next = newNode;
	newNode->prev = temp;
	text.tail = newNode;
}

void DeleteList(struct list_node *head)
{
	struct list_node* temp = head; 
	struct list_node* next = head;
	while(temp != NULL)
	{
		free(temp -> str);
		next = next -> next;
		free(temp);
		temp = next;
	}
	text.head = NULL;
	text.tail = NULL;
	text.num_elements = 0;
	text.x = 0;
	text.y = 0;
	text.curr = NULL;
}

void MakeMeListString(wchar_t *str)
{
	int i = 0, j = 0;
	wchar_t *s;
	while(1)
	{
		s = NULL;	
		while(str[i] != '\n' && str[i] != '\0')
		{
			s = realloc(s, (j + 1) * sizeof(wchar_t));
			s[j] = str[i];
			j++;
			i++;
		}
		if(str[i] == L'\0')
		{
			s = realloc(s, (j + 2) * sizeof(wchar_t));
			s[j] = L' ';
			s[j + 1] = L'\0';
			InsertAtTail(s);
			break;
		}
		else
		{
			s = realloc(s, (j + 2) * sizeof(wchar_t));
			s[j] = L'\n';
			s[j + 1] = L'\0';
			j = 0;
			i++;
			InsertAtTail(s);
		}
	}
	text.curr = text.head;
}

void MakeMeList(FILE *fp)
{
	int i = 0, j = 0, pos = 0, s;
	wchar_t *str = NULL, c;	
	if(fp)
	{
		while((c = (fgetwc(fp))) != WEOF)
		{	
			i++;
			if (c == '\n')
			{	
				str = malloc(i * sizeof(wchar_t) + sizeof(wchar_t));
				fseek(fp, pos, SEEK_SET);
				fgetws(str, i + 1, fp);
				InsertAtTail(str);
				j++;
				i = 0;
				pos = ftell(fp);
			}	
		}
		str = malloc(i * sizeof(wchar_t) + 2 * sizeof(wchar_t));
		fseek(fp, pos, SEEK_SET);
		fgetws(str, i + 1, fp);
		str[i] = L' ';
		str[i + 1] = L'\0';
		InsertAtTail(str);
	}
}

void MakeYTop()
{
	text.terminalbottom = text.terminalbottom - text.terminaltop + text.y;
	text.terminaltop = text.y;
}

void OutputFix(int x, int y)
{
	if(text.x >= (text.terminalright - x)) 
	{
		text.terminalleft = text.terminalleft + text.x - text.terminalright + x + 1;
		text.terminalright = text.terminalright + text.x - text.terminalright + x + 1;
	}
	if(text.x < (text.terminalleft))
	{		
		text.terminalright = text.terminalright - (text.terminalleft - text.x);
		text.terminalleft = text.terminalleft - (text.terminalleft - text.x);
	}
	if(text.y >= text.terminalbottom - y)
	{	
		text.terminaltop = text.terminaltop + text.y - text.terminalbottom + 1 + y;
		text.terminalbottom = text.terminalbottom + text.y - text.terminalbottom + 1 + y;
	}
	if(text.y < text.terminaltop)
	{
		text.terminalbottom = text.terminalbottom - (text.terminaltop - text.y);
		text.terminaltop = text.terminaltop - (text.terminaltop - text.y);
	}
}

void OutputMyList(struct list_node *head)
{
	wprintf(CLRSCR);
	int i = 0, j = 0, k = 0, x = 0, y = 0;
	struct list_node* temp;
	temp = head;
	if(text.mode == 0) 
	{
		OutputFix(1, 1);
		x = 1;
		y = 1;
	}
	else if(text.mode == 1) 
	{
		x = Prefix(text.num_elements) + 2;
		OutputFix(x, 1);
		y = 1;
	}
	for(i = 0; (i < text.terminalbottom - y) && i < text.num_elements; i++)
	{		
		if(text.mode == 1)
		{
			for(k = 0; k < (Prefix(text.num_elements) - Prefix(i + 1)); k++) wprintf(L" "); 
			wprintf(L"%d:", i + 1);
		}
		if (text.terminalleft != 0)	wprintf(L"<");
		else wprintf(L"|");
		for(j = text.terminalleft; j < wcslen(temp -> str) && (j < text.terminalright - x); j++)
		{
			if (i == text.y && j == text.x) 
				{
					if (temp->str[j] == '\n')
					{
						wprintf(COLOR L" " RESET);
						wprintf(L"\n");
					}
					else
					{
						wprintf(COLOR L"%lc" RESET, temp -> str[j]);
					}
				}
			else wprintf(L"%lc", temp -> str[j]);
		}
		if(j == text.terminalleft) wprintf(L"\n");
		else if(temp -> str[j - 1] != '\n') wprintf(L"\n");
		temp = temp -> next;
	}
	while(i < text.terminalbottom - y)
		{
			wprintf(L"\n");
			i++;
		}
	//wprintf(L"x = %d y = %d tb = %d tt = %d tr = %d tl = %d nl = %d", text.x, text.y, text.terminalbottom, text.terminaltop, text.terminalright, text.terminalleft, text.num_elements);
	wprintf(L"%d(%d):", text.y + 1, text.num_elements);
}

void ChangeCursorPosition(char dir)
{
	switch(dir)
	{
		case 1 : 
		if(text.y == 0) break;		
		text.curr = text.curr -> prev;	
		text.y--;
		if(wcslen(text.curr -> str) < text.x + 1)
		{
			text.x = wcslen(text.curr -> str) - 1;			
			if(text.x < 0) text.x = 0;
			break;
		}
		break;
		case 2 :
		if(text.y == text.num_elements - 1) break;
		text.curr = text.curr -> next;
		text.y++;
		if(wcslen(text.curr -> str) < text.x + 1)
		{
			text.x = wcslen(text.curr -> str) - 1;
			if(text.x < 0) text.x = 0;
			break;
		}
		break;
		case 3 :
		if(text.x == (wcslen(text.curr -> str) - 1))
		{
			if(text.curr -> next == NULL) break;
			text.x = 0;
			ChangeCursorPosition(2);
			break;
		}
		text.x++;
		break;
		case 4 :
		if(text.x == 0)
		{
			if(text.curr -> prev == NULL) break;
			text.x = wcslen(text.curr -> prev -> str) - 1;
			ChangeCursorPosition(1);
			break;
		}
		text.x--;
		break;
	}
}

int Prefix(int x)
{
	int i = 0, j;
	j = x;
	do
	{
		i++;
		j = j / 10;
	}while(j != 0);
	return i;
}

struct winsize CheckTerminalSize(struct winsize w)
{
	struct winsize new;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &new);
	if(new.ws_col != w.ws_col || new.ws_row != w.ws_row)
	{
		text.terminalright = text.terminalright + (new.ws_col - w.ws_col);
		text.terminalbottom = text.terminalbottom + (new.ws_row - w.ws_row);
		return new;
	}
	else return w;
}

void Number(int y)
{
	int i, curry = text.y;
	if(y == 0)
	{
		text.y = 0;
		text.curr = text.head;
	}
	else if(y > text.num_elements - 1)
	{
		text.y = text.num_elements - 1;
		text.curr = text.tail;
	}
	else 
	{
		if(text.y > y - 1) for(i = 0; i < (curry - y + 1); i++) ChangeCursorPosition(1);
		else for(i = 0; i < (y - 1 - curry); i++) ChangeCursorPosition(2);
	}
	MakeYTop();
	text.window = CheckTerminalSize(text.window);
	OutputMyList(text.head);
	text.x = 0;
}

char Search(wchar_t *str)
{
	int i = 0;
	struct list_node* temp = text.curr;
	wchar_t *s =  temp -> str;
	s = s + text.x;
	if (wcsstr(s, str) != NULL) 
	{
		text.x = text.x + wcsstr(s, str) - s;
		MakeYTop();					
		text.window = CheckTerminalSize(text.window);
		OutputMyList(text.head);
		return 0;
	}
	else
	{
		temp = temp -> next;
		while(temp != NULL)
		{
			s = temp -> str;
			i++;
			if (wcsstr(s, str) != NULL) 
			{
				for(i = i; i > 0; i--) ChangeCursorPosition(2);
				text.x = wcsstr(s, str) - s;
				MakeYTop();							
				text.window = CheckTerminalSize(text.window);
				OutputMyList(text.head);
				return 0;
			}
			temp = temp -> next;
		}
	}
	return 1;
}

Twochar_w Escape(wchar_t *str)
{
	wchar_t *s1 = NULL, *s2 = NULL;
	Twochar_w t;
	t.s1 = NULL;
	t.s2 = NULL;
	static int arr[2] = { 0, 0 };
	int i, j = 0;
	char check = 1;
	if(str[0] != L'/' || (str[1] == L'/' && str[2] == L'\0') || str[wcslen(str) - 1] != '/') return t;
	for(i = 1; i < wcslen(str); i++)
	{
		if(check == 1)
		{
			s1 = realloc(s1, (j + 1) * sizeof(wchar_t));
		}
		else
		{
			s2 = realloc(s2, (j + 1) * sizeof(wchar_t));
		}
		if(str[i] == L'/')
		{
			switch(check)
			{
				case 1:
				if(i + 1 == wcslen(str))
				{
					free(s1);
					//free(s2);
					t.s1 = NULL;
					return t;
				}				
				s1[j] = L'\0';
				t.s1 = s1;
				check--;
				j = -1;
				break;
				case 0:
				if(str[i + 1] == L'\0')
				{
					s2[j] = L'\0';
					t.s2 = s2;
					return t;
				}
				else 
				{
					free(s1);
					free(s2); 
					t.s1 = NULL;
					return t;
				}
			}
		}
		else if(str[i] == '\\')
		{
			if(str[i + 1] == '\0') 
			{

				free(s1);
				free(s2); 
				t.s1 = NULL;
				return t;
			}
			switch(str[i + 1])
			{
				case L'\\':
				if(check == 1) s1[j] = L'\\';
				else s2[j] = L'\\';
				i++;
				break;
				case L'/':
				if(check == 1) s1[j] = L'/';
				else s2[j] = L'/';
				i++;
				break;
				case L'n':
				if(check == 1) s1[j] = L'\n';
				else s2[j] = L'\n';
				i++;
				break;
				default:
				free(s1);
				free(s2);
				t.s1 = NULL; 
				return t;
			}
		}
		else 
		{
			if(check == 1) s1[j] = str[i];
			else s2[j] = str[i];
		}
		j++;
	}
	if (t.s1 != NULL)free(t.s1);
	t.s1 = NULL;
	//if (t.s2 != NULL)free(t.s2); 
	return t;
}

wchar_t* MakeMeString(struct list_node *head)
{
	struct list_node* temp = head;
	wchar_t* str = NULL;
	int j = 0, i = 0;
	while(temp != NULL)
	{
		j = 0;
		while(temp -> str[j] != L'\n' && temp -> str[j] != L'\0')
		{
			str = realloc(str, (i + 1) * sizeof(wchar_t));
			str[i] = temp -> str[j];
			i++;
			j++;
		}
		if(temp -> str[j] == L'\0')
		{
			str[i - 1] = '\0';
		}
		else
		{
			str = realloc(str, (i + 1) * sizeof(wchar_t));
			str[i] = L'\n';
			i++;
		}
		temp = temp -> next;
	}
	return str;
}

char DeleteEmptyLines(struct list_node *head)
{
	struct list_node* temp = head; 
	struct list_node* next = head;
	char res = 0;
	if(text.head -> next == NULL) return 1;
	while(temp -> next != NULL)
	{
		next = temp -> next;
		if(wcslen(temp -> str) == 1)
		{
			if(res == 0) res = 1;
			free(temp -> str);
			if(temp -> prev == NULL) 
			{
				text.head = temp -> next;
				text.head -> prev = NULL;
				free(temp);
				temp = next;
				text.num_elements--;
			}
			else
			{
				temp -> prev -> next = temp -> next;
				temp -> next -> prev = temp -> prev;
				free(temp);				
				temp = next;
				text.num_elements--;
			}
		}
		else temp = temp -> next;
	}
	if(wcslen(temp -> str) == 1)
	{
		if(temp -> prev != NULL)
		{
			if(res == 0) res = 1;
			free(temp -> str);
			text.tail = temp -> prev;
			text.tail -> next = NULL;
			free(temp);
			text.num_elements--;
			text.tail -> str[wcslen(text.tail -> str) - 1] = L' ';
		}
	}
	if(res == 1)
	{
		text.curr = text.head;
		text.x = 0;
		text.y = 0;
	}
	return res;
}

void Subst(wchar_t *str)
{
	int *iarr = NULL;
	int i = 0, arrlength = 0, k = 0, l = 0;
	Twochar_w t;
	wchar_t *s1 = NULL, *s2 = NULL, *s = NULL;
	t = Escape(str);
	if(t.s1 == NULL)
	{
		text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
		wprintf(L"Error in input");
		return;
	}
	s1 = t.s1;
	s2 = t.s2;
	
	if(wcslen(s1) == 0)
	{
		text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
		wprintf(L"Nothing to find");
		free(s1);
		free(s2);
		return;
	}
	if(wcslen(s1) == 1 && s1[0] == L'\n' && wcslen(s2) == 0)
	{
		if(DeleteEmptyLines(text.head) == 1)
		{
			text.window = CheckTerminalSize(text.window);
   			OutputMyList(text.head);
			free(s1);
			free(s2);
			wprintf(L"Empty lines deleted");
   			return;
   		}
   		text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
   		wprintf(L"Nothing found");
   		free(s1);
		free(s2);
		return;
	}
	if((s = MakeMeString(text.head)) == NULL)
	{
		text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
   		free(s1);
		free(s2);
		free(s);
   		wprintf(L"Nothing found");
   		return;
	}
	str = s;

	if(wcsstr(s, s1) == NULL)
	{
		text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
   		free(s1);
		free(s2);
		free(str);
   		wprintf(L"Nothing found");
   		return;		
	}
	while((s = wcsstr(s, s1)) != NULL)
	{	
		i++;
		iarr = realloc(iarr, i * sizeof(int));
		iarr[i - 1] = s - str;
		if(wcslen(s) < wcslen(s1)) break;
		s = s + wcslen(s1);
		wprintf(L"\nSubst working: found matches %d", i);
	}
	s = NULL;
	arrlength = i;
	for(i = 0; i <= wcslen(str) + (wcslen(s2) - wcslen(s1)) * arrlength; i++)
	{			
		if(k < arrlength && l == iarr[k])
		{
			if(wcslen(s2) != 0)
			{
				s = realloc(s, (i + wcslen(s2) + 1) * sizeof(wchar_t));
				s[i + wcslen(s2)] = L'\0';
				wcscat(s, s2);
			}
			l = l + wcslen(s1);
			i = i + wcslen(s2) - 1;
			k++;
			wprintf(L"\nSubst working: replaced matches %d out of %d", k, arrlength);
		}
		else
		{
			s = realloc(s, (i + 2) * sizeof(wchar_t));
			s[i] = str[l];
			s[i + 1] = '\0';
			l++;
		}
	}
	DeleteList(text.head);
	MakeMeListString(s);
   	free(s1);
	free(s2);
	free(str);
	free(s);
	free(iarr);
	text.window = CheckTerminalSize(text.window);
	OutputMyList(text.head);
	wprintf(L"Replaced matches %d", arrlength);
	return;
}

void WriteToFile(wchar_t* str)
{
	int i;
	FILE *output;
	wchar_t* s;
	if(wcslen(str) < 3 || str[0] != L'"' || str[wcslen(str) - 1] != L'"')
	{
		text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
   		wprintf(L"Wrong input");
   		return;
	}

	char filename[wcslen(str) - 1];

	for(i = 1; i < wcslen(str) - 1; i++) filename[i - 1] = str[i];
	filename[i - 1] = L'\0';
	output = fopen(filename, "w");
	s = MakeMeString(text.head);
	fputws(s, output);
	fclose(output);
	free(s);
	text.window = CheckTerminalSize(text.window);
	OutputMyList(text.head);
	wprintf(L"Write completed");
}

char CommandCenter(wchar_t symbol)
{
	int i = 1;
	wchar_t *cmd = malloc(sizeof(wchar_t));
	while(symbol != '\n')
	{	
		if(symbol == 004) return 1;
		if(symbol == 127) symbol = getwchar();
		else if(symbol >= ' ')
		{	
			i++;	
			putwchar(symbol);
			cmd = realloc(cmd, i * sizeof(wchar_t));
			cmd[i - 2] = symbol;
			cmd[i - 1] = '\0';
			symbol = getwchar();
		}
		else 
		{
			getwchar();
			getwchar();
			symbol = getwchar();
		}
	}
	if(cmd[0] == '/')
	{
		if(cmd[1] == '\0')
		{
			if(text.prevsearch == NULL)
			{
				text.window = CheckTerminalSize(text.window);
				OutputMyList(text.head);
				wprintf(L"Nothing to find");
				free(cmd);
				return 0;
			}
			ChangeCursorPosition(3);
			if(Search(text.prevsearch) == 1)
			{
				ChangeCursorPosition(4);
				text.window = CheckTerminalSize(text.window);
				OutputMyList(text.head);
				wprintf(L"Nothing found:(");
			}
			free(cmd);
			return 0;
		}
		free(text.prevsearch);
		text.prevsearch = malloc(sizeof(wchar_t) * (wcslen(cmd)));
		for(i = 1; i <= wcslen(cmd); i++) text.prevsearch[i - 1] = cmd[i];
		if(Search(text.prevsearch) == 1)
		{
			text.window = CheckTerminalSize(text.window);
			OutputMyList(text.head);
			wprintf(L"Nothing found:(");
		}
		free(cmd);
		return 0;
	}
	for(i = 0; i < wcslen(cmd); i++)
	{
		if(cmd[i] < '0' || cmd[i] > '9')
		{
			i = -1;
			break;
		}
	}
	if(i > -1) 
	{
		Number(wcstol(cmd, NULL, 10));
		free(cmd);
		return 0;
	}
	if(cmd == wcsstr(cmd, L"subst "))
	{				
		Subst(cmd + 6);
		free(cmd);
		return 0;
	}
	if(cmd == wcsstr(cmd, L"write "))
	{		
		WriteToFile(cmd + 6);
		free(cmd);
		return 0;		
	}
	free(cmd);
    text.window = CheckTerminalSize(text.window);
   	OutputMyList(text.head);
	wprintf(L"unknown command");
}

void print_usage()
{
	 wprintf(L"Usage: -v/-h/-n filename\nv - version\nh - manual\nn - line numbers\n");
}

int main(int argc, char *argv[])
{	
    if(!isatty(0))
    {
        wprintf(L"Redirected input\nProgramm is usleess\n");
        return 1;  
    }
	setlocale(LC_ALL, "en_US.utf8");
	int symbol, option = 0, i = 1;
	wchar_t ch, *s;
	FILE *input;
	struct winsize w;
	text.mode = 0;
	while ((option = getopt(argc, argv,"nhv")) != -1) {
        switch (option) {
             case 'n' : text.mode = 1;
             	i++;
             	if(getopt(argc, argv,"nhv") != -1)
             	{
             		print_usage();             		
                 	exit(EXIT_FAILURE);
             	}
                 break;
             case 'h' :
             	if(getopt(argc, argv,"nhv") != -1)
             	{
             		print_usage();             		
                 	exit(EXIT_FAILURE);
             	}
             	wprintf(L"subst /[s1]/[s2]/ - find s1 replace with s2\n/[string] - find sub string s\n/ - find previous search \n[number] - go to line number\nwrite [file name] - save to file name\nq - quit\n");
             	return 0;
                break;
             case 'v' :
            	if(getopt(argc, argv,"nhv") != -1)
             	{
             		print_usage();             		
                 	exit(EXIT_FAILURE);
             	}
             	wprintf(L"Less: alpha version 0.1.3.37\n");
             	return 0;
                break;
             default: print_usage(); 
                exit(EXIT_FAILURE);
        }
    }
  	if((input = fopen(argv[i], "r")) == NULL)
  	{
  		wprintf(L"Can not open %s\n", argv[i]);
  		return 0;
  	}    
	MakeMeList(input);
	fclose(input);
	if(!isatty (1))
	{
		s = MakeMeString(text.head);
		wprintf(s);
		free(s);
		DeleteList(text.head);
		return 0;
	}
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	struct termios old_attributes,new_attributes;
    tcgetattr(0,&old_attributes);
    memcpy(&new_attributes,&old_attributes,sizeof(struct termios));   
    new_attributes.c_lflag &= ~ECHO;
    new_attributes.c_lflag &= ~ICANON;
    new_attributes.c_cc[VMIN] = 1;
    tcsetattr(0,TCSANOW,&new_attributes);
	text.prevsearch = NULL;
	text.curr = text.head;	
	text.x = 0;
	text.y = 0;
	text.terminaltop = 0;
	text.terminalleft = 0;
	text.terminalright = w.ws_col;
	text.terminalbottom = w.ws_row;
	text.window = w;
	OutputMyList(text.head);
    while((symbol = getwchar()) != 004)
    {
    	if(symbol == 'q') break;
    	text.window = CheckTerminalSize(text.window);
   		OutputMyList(text.head);
       	if (symbol == 27)
    	{
    		getwchar();
    		symbol = getwchar();
   			if(symbol == 'A') ChangeCursorPosition(1);//UP
   			if(symbol == 'B') ChangeCursorPosition(2);//DOWN
   			if(symbol == 'C') ChangeCursorPosition(3);//RIGHT
   			if(symbol == 'D') ChangeCursorPosition(4);//LEFT    			
    		text.window = CheckTerminalSize(text.window);
   			OutputMyList(text.head);
    	}
    	else if(symbol != '\n') if(CommandCenter(symbol) == 1) break;
    }
    free(text.prevsearch);
    DeleteList(text.head);
	tcsetattr(0,TCSANOW,&old_attributes);
	wprintf(CLRSCR);
	return 0;
}
