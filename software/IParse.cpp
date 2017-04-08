
#define VERSION "1.7 of April 9, 2015."

/* 
	First some standard definitions.
*/

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
//#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "Ident.h"
#include "String.h"
#include "AbstractParseTree.h"
#include "TextFileBuffer.h"
#include "Scanner.h"
#include "ProtosScanner.h"
#include "RcScanner.h"
#include "PascalScanner.h"
#include "AbstractParser.h"
#include "BTParser.h"
#include "BTHeapParser.h"
#include "LL1Parser.h"
#include "LL1HeapParser.h"
#include "ParParser.h"
#include "TextReader.h"
#include "XMLParser.h"
#include "Unparser.h"

#define ASSERT assert




class SContext
{
public:
	SContext(const char* name)
	{
		_next = _top;
		_top = this;
		_name = name;
	}
	~SContext()
	{
		_top = _next;
	}

	static void print(FILE* f)
	{
		if (_top)
			_top->_print(f);
	}

private:
	void _print(FILE* f)
	{
		if (_next)
			_next->_print(f);

		fprintf(f, "%s:", _name);
	}
	SContext* _next;
	const char* _name;
	static SContext* _top;
};

SContext* SContext::_top = 0;






/*
	Function for printing the Abstract Program Tree
	in the form of a function, which after being
	called, returns the reconstruction of the tree.
	
	This function can be used to hard-code a parser
	for a given grammar. This function was used to
	produce the function init_IParse_grammar below, 
	which creates the Abstract Program Tree representing
	the input grammar of IParse.
*/

int print_tree_depth = 0;
static void print_tree_rec( FILE *f, const AbstractParseTree& tree );
 
void print_tree_to_c( FILE *f, const AbstractParseTree& tree, char *name )
{   
    fprintf(f, "void init_%s( AbstractParseTree& root )\n", name);
    fprintf(f, "{   /* Generated by IParse version %s */\n", VERSION);
    fprintf(f, "    AbstractParseTree tt[100];\n");
    fprintf(f, "    int v = 0;\n");
    fprintf(f, "\n");
    fprintf(f, "#define NONE tt[v-1].appendChild(AbstractParseTree());\n");
    fprintf(f, "#define ID(I) tt[v-1].appendChild(Ident(I));\n");
    fprintf(f, "#define VAL(V) tt[v-1].appendChild(V);\n");
    fprintf(f, "#define TREE(N) tt[v++].createTree(N);\n");
    fprintf(f, "#define LIST tt[v++].createList();\n");
    fprintf(f, "#define CLOSE if (--v > 0) tt[v-1].appendChild(tt[v]);\n");
    fprintf(f, "\n    ");

    print_tree_rec( f, tree );
    fprintf(f, "\n    root = tt[0];\n}\n\n");
}

static void print_tree_rec( FILE *f, const AbstractParseTree& tree )
{
    if (tree.isEmpty())
         fprintf(f, "NONE ");
    else if (tree.isIdent())
    {    fprintf(f, "ID(\"%s\") ", tree.identName().val());
    }
    else if (tree.isString())
    {    fprintf(f, "VAL(\"%s\") ", tree.stringValue());
    }
    else if (tree.isInt())
    {    fprintf(f, "VAL(%d) ", tree.intValue());
    }
    else if (tree.isDouble())
    {    fprintf(f, "VAL((double)%f) ", tree.doubleValue());
    }
    else if (tree.isChar())
    {    fprintf(f, "VAL('%c') ", tree.charValue());
    }
    else
    {   
        if (tree.isList())
            fprintf(f, "LIST ");
        else
            fprintf(f, "TREE(\"%s\") ", tree.type());
        int nr_parts = tree.nrParts();
        if (nr_parts > 0)
        {   print_tree_depth++;

            for (AbstractParseTree::iterator child_it(tree); child_it.more(); child_it.next())
            {   
            	print_tree_rec(f, child_it);
            	if (--nr_parts > 0)
                	fprintf(f, "\n%*.*s    ",
                          print_tree_depth, print_tree_depth, "");
            }
            print_tree_depth--;
        }
        fprintf(f, "CLOSE ");
    }
};


static void print_string_to_xml( FILE *f, const char *s)
{
	for (; *s != '\0'; s++)
		if (*s == '<')
			fprintf(f, "&lt;");
		else if (*s == '>')
			fprintf(f, "&gt;");
		else if (*s == '&')
			fprintf(f, "&amp;");
		else
			fprintf(f, "%c", *s);
}

static void print_tree_to_xml( FILE *f, const AbstractParseTree& tree )
{
	fprintf(f, "\n%*.*s", print_tree_depth, print_tree_depth, "");
    if (tree.isEmpty())
         fprintf(f, "<EMPTY/>");
    else if (tree.isIdent())
    {   fprintf(f, "<ID>");
    	print_string_to_xml(f, tree.identName().val());
    	fprintf(f, "</ID>");
    }
    else if (tree.isString())
    {   fprintf(f, "<STRING>");
    	print_string_to_xml(f, tree.identName().val());
    	fprintf(f, "</STRING>");
    }
    else if (tree.isInt())
    	fprintf(f, "<INT>%d</INT>", tree.intValue());
    else if (tree.isDouble())
		fprintf(f, "<DOUBLE>%f</DOUBLE>", tree.doubleValue());
    else if (tree.isChar())
	{
		fprintf(f, "<CHAR>");
		char str[2];
		str[0] = tree.charValue();
		str[1] = '\0';
    	print_string_to_xml(f, str);
    	fprintf(f, "</CHAR>");
    }
    else
    {   
        if (tree.nrParts() > 0)
	    {
	        if (tree.isList())
	            fprintf(f, "<LIST>");
	        else
	            fprintf(f, "<TREE TYPE=\"%s\">", tree.type());

		    print_tree_depth++;
            for (AbstractParseTree::iterator child_it(tree); child_it.more(); child_it.next())
            	print_tree_to_xml(f, child_it);
            print_tree_depth--;
	        if (tree.isList())
	            fprintf(f, "</LIST>");
	        else
	            fprintf(f, "</TREE>", tree.type());
	    }
	    else
	    {
	        if (tree.isList())
	            fprintf(f, "<LIST/>");
	        else
	            fprintf(f, "<TREE TYPE=\"%s\"/>", tree.type());
	    }
    }
};




/*
	Initialization of the IParse grammar
	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Below the procedure that initializes the Abstract Program Tree
	for the input grammar of IParse itself is given. This was
	generated by calling the procedure print_tree_to_c above.
*/
void init_IParse_grammar( AbstractParseTree& root )
{   /* Generated by IParse version 1.6 of March 15, 2015. */
    AbstractParseTree tt[100];
    int v = 0;

#define NONE tt[v-1].appendChild(AbstractParseTree());
#define ID(I) tt[v-1].appendChild(Ident(I));
#define VAL(V) tt[v-1].appendChild(V);
#define TREE(N) tt[v++].createTree(N);
#define LIST tt[v++].createList();
#define CLOSE if (--v > 0) tt[v-1].appendChild(tt[v]);

    LIST TREE("nt_def") ID("root") 
      LIST TREE("rule") LIST TREE("seq") ID("nt_def") 
          NONE CLOSE 
         ID("eof") CLOSE 
        NONE CLOSE CLOSE CLOSE 
     TREE("nt_def") ID("nt_def") 
      LIST TREE("rule") LIST ID("ident") 
         TREE("literal") VAL(":") 
          NONE CLOSE 
         ID("or_rule") 
         TREE("literal") VAL(".") 
          NONE CLOSE CLOSE 
        ID("nt_def") CLOSE CLOSE CLOSE 
     TREE("nt_def") ID("or_rule") 
      LIST TREE("rule") LIST TREE("chain") ID("rule") 
          NONE 
          VAL("|") CLOSE CLOSE 
        NONE CLOSE CLOSE CLOSE 
     TREE("nt_def") ID("rule") 
      LIST TREE("rule") LIST TREE("opt") TREE("seq") ID("opt_elem") 
           NONE CLOSE 
          NONE CLOSE 
         TREE("opt") LIST TREE("rule") LIST TREE("literal") VAL("[") 
              NONE CLOSE 
             ID("ident") 
             TREE("literal") VAL("]") 
              NONE CLOSE CLOSE 
            NONE CLOSE CLOSE 
          NONE CLOSE CLOSE 
        ID("rule") CLOSE CLOSE CLOSE 
     TREE("nt_def") ID("opt_elem") 
      LIST TREE("rule") LIST ID("list_elem") 
         TREE("literal") VAL("OPT") 
          NONE CLOSE 
         LIST TREE("rule") LIST TREE("literal") VAL("AVOID") 
             NONE CLOSE CLOSE 
           ID("avoid") CLOSE 
          TREE("rule") LIST TREE("literal") VAL("NONGREEDY") 
             NONE CLOSE CLOSE 
           ID("nongreedy") CLOSE 
          TREE("rule") NONE 
           NONE CLOSE CLOSE CLOSE 
        ID("opt") CLOSE 
       TREE("rule") LIST ID("list_elem") CLOSE 
        NONE CLOSE CLOSE CLOSE 
     TREE("nt_def") ID("list_elem") 
      LIST TREE("rule") LIST ID("prim_elem") 
         TREE("literal") VAL("SEQ") 
          NONE CLOSE 
         LIST TREE("rule") LIST TREE("literal") VAL("AVOID") 
             NONE CLOSE CLOSE 
           ID("avoid") CLOSE 
          TREE("rule") NONE 
           NONE CLOSE CLOSE CLOSE 
        ID("seq") CLOSE 
       TREE("rule") LIST ID("prim_elem") 
         TREE("literal") VAL("LIST") 
          NONE CLOSE 
         LIST TREE("rule") LIST TREE("literal") VAL("AVOID") 
             NONE CLOSE CLOSE 
           ID("avoid") CLOSE 
          TREE("rule") NONE 
           NONE CLOSE CLOSE CLOSE 
        ID("list") CLOSE 
       TREE("rule") LIST ID("prim_elem") 
         TREE("literal") VAL("CHAIN") 
          NONE CLOSE 
         LIST TREE("rule") LIST TREE("literal") VAL("AVOID") 
             NONE CLOSE CLOSE 
           ID("avoid") CLOSE 
          TREE("rule") NONE 
           NONE CLOSE CLOSE 
         ID("string") CLOSE 
        ID("chain") CLOSE 
       TREE("rule") LIST ID("prim_elem") CLOSE 
        NONE CLOSE CLOSE CLOSE 
     TREE("nt_def") ID("prim_elem") 
      LIST TREE("rule") LIST ID("string") 
         TREE("opt") LIST TREE("rule") LIST TREE("literal") VAL("*") 
              NONE CLOSE CLOSE 
            ID("local") CLOSE CLOSE 
          NONE CLOSE CLOSE 
        ID("literal") CLOSE 
       TREE("rule") LIST ID("ident") CLOSE 
        NONE CLOSE 
       TREE("rule") LIST TREE("literal") VAL("ident") 
          NONE CLOSE 
         TREE("literal") VAL(">+") 
          NONE CLOSE 
         ID("ident") CLOSE 
        ID("identdefadd") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("ident") 
          NONE CLOSE 
         TREE("literal") VAL(">") 
          NONE CLOSE 
         ID("ident") CLOSE 
        ID("identdef") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("ident") 
          NONE CLOSE 
         TREE("literal") VAL("<") 
          NONE CLOSE 
         ID("ident") CLOSE 
        ID("identuse") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("ident") 
          NONE CLOSE 
         TREE("literal") VAL("!") 
          NONE CLOSE 
         ID("ident") CLOSE 
        ID("identfield") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("ident") 
          NONE CLOSE CLOSE 
        ID("identalone") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("{") 
          NONE CLOSE CLOSE 
        ID("opencontext") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("}") 
          NONE CLOSE CLOSE 
        ID("closecontext") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("\\") 
          NONE CLOSE 
         ID("ident") CLOSE 
        ID("wsterminal") CLOSE 
       TREE("rule") LIST TREE("literal") VAL("(") 
          NONE CLOSE 
         ID("or_rule") 
         TREE("literal") VAL(")") 
          NONE CLOSE CLOSE 
        NONE CLOSE CLOSE CLOSE CLOSE 
	root = tt[0];
}


/*
	Symbol tables
	~~~~~~~~~~~~~
	The following section of code deals with the storage of
	symbol tables, and resolving all identifiers with their
	definitions.
*/

typedef struct ident_def_t ident_def_t, *ident_def_p;
typedef struct context_entry_t context_entry_t, *context_entry_p;

struct ident_def_t
{	ident_def_p next;
	tree_t *tree;
};

struct context_entry_t
{	context_entry_p next;
	const char *ident_name;
	const char *ident_class;
	ident_def_p defs;
};


int main(int argc, char *argv[])
{   int i;

	bool debug_nt = false;
	bool debug_parse = false;
	bool debug_scan = false;
	bool silent = false;
	const char* constBTStack = "BTStack";
	const char* constBTHeap = "BTHeap";
	const char* constLL1Stack = "LL1Stack";
	const char* constLL1Heap = "LL1Heap";
	const char* constPar = "Par";
	const char* constBasic = "Basic";
	const char* constWhiteSpace = "WhiteSpace";
	const char* constProtos = "Protos";
	const char* constResource = "Resource";
	const char* constPascal = "Pascal";
	const char* constColourCoding = "ColourCoding";
	const char* constRaw = "Raw";
	const char* constBare = "Bare";
	const char *selected_parser = constBTStack;
	const char* use_scanner = constBasic;

    if (argc == 1)
    {   printf("Usage: %s <grammar-file> <input-file>\n"
               "\n"
               "  options\n"
			   "   -plain      read input file verbatim (default)\n"
			   "   -cp1252     read input file as code page 1252\n"
			   "   -utf16      read input file as UTF-16\n"
			   "   -BTStack    use back-tracking stack parser\n"
			   "   -BTHeap     use back-tracking heap parser\n"
			   "   -LL1Stack   use LL1 stack parser\n"
			   "   -Par        use parallel parser\n"
			   "   -WhiteSpace use white space scanner\n"
			   "   -Protos     use Protos scanner\n"
			   "   -Resource   use Pesource scanner\n"
			   "   -Pascal     use Pascal scanner\n"
			   "   -ColourCoding use colour coding scanner\n"
			   "   -Raw        use raw scanner\n"
			   "   -Bare       use bare scanner\n"
			   "   -p <fn>     print parse tree\n"
               "   -xml <fn>   output parse tree as XML\n"
               "   -o <fn>     ouput tree to C file\n"
			   "   -unparse <fn> unparses the parse tree\n"
               "   +ds         debug scanning (full)\n"
               //"   +dss      debug scanning (normal)\n"
               "   -ds         no debug scanning\n"
               "   +dp         debug parsing\n"
               "   -dp         no debug parsing\n"
               "   +dn         debug non-terminals\n"
               "   -dn         no debug non-terminals\n", argv[0]);
        return 0;
    }

	PlainFileReader plainFileReader;
	CodePage1252 codePage1252;
	CodePageToUF8ConverterStream codePage1252ToUF8ConverterStream(codePage1252);
	ConverterFileReader codePage1252FileReader(codePage1252ToUF8ConverterStream);
	UTF16ToUTF8ConverterStream utf16ToUTF8ConverterStream;
	ConverterFileReader utf16FileReader(utf16ToUTF8ConverterStream);
	AbstractFileReader *selected_file_reader = &plainFileReader;

	UTF8ToCodePageConverterStream utf8ToCodePageConverterStream(codePage1252);
	UTF8ToUTF16ConverterStream utf8ToUTF16ConverterStream;
	ConverterStream<char, char> *selected_output_converter = 0;
	
	AbstractParseTree grammarTree;
	AbstractParseTree tree;
	init_IParse_grammar(tree);

    for (i = 1; i < argc; i++)
    {   char *arg = argv[i];
 
        if (!strcmp(arg, "-s"))
			silent = true;
		
		if (!silent)
		{
			if (i == 1)
			    fprintf(stderr, "Iparse, Version: %s\n", VERSION);
        	printf("Processing: %s\n", arg); 
		}
		
        //if (!strcmp(arg, "+g"))
        //    grammar_level++;
        if (!strcmp(arg, "-s"))
			;
		else if (!strcmp(arg, "-plain"))
		{
			selected_file_reader = &plainFileReader;
			selected_output_converter = 0;
		}
		else if (!strcmp(arg, "-cp1252"))
		{
			selected_file_reader = &codePage1252FileReader;
			selected_output_converter = &utf8ToCodePageConverterStream;
		}
		else if (!strcmp(arg, "-utf16"))
		{
			selected_file_reader = &utf16FileReader;
			selected_output_converter = &utf8ToUTF16ConverterStream;
		}
		else if (!strcmp(arg, "-BTStack"))
			selected_parser = constBTStack;
        else if (!strcmp(arg, "-BTHeap"))
			selected_parser = constBTHeap;
		else if (!strcmp(arg, "-LL1Stack"))
			selected_parser = constLL1Stack;
		else if (!strcmp(arg, "-LL1Heap"))
			selected_parser = constLL1Heap;
		else if (!strcmp(arg, "-Par"))
			selected_parser = constPar;
        else if (!strcmp(arg, "-WhiteSpace"))
		{
			use_scanner = constWhiteSpace;
		}
        else if (!strcmp(arg, "-Protos"))
		{
			use_scanner = constProtos;
		}
        else if (!strcmp(arg, "-Resource"))
		{
			use_scanner = constResource;
		}
        else if (!strcmp(arg, "-Pascal"))
		{
			use_scanner = constPascal;
		}
        else if (!strcmp(arg, "-ColourCoding"))
		{
			use_scanner = constColourCoding;
		}
        else if (!strcmp(arg, "-Raw"))
		{
			use_scanner = constRaw;
		}
        else if (!strcmp(arg, "-Bare"))
		{
			use_scanner = constBare;
		}
        else if (!strcmp(arg, "-p"))
        {   
            char *file_name = argv[++i];
            FILE *fout = !strcmp(file_name, "-") 
                         ? stdout : fopen(file_name, "wt");

            if (fout != 0)
            {   char *dot = strstr(file_name, ".");
                if (dot)
                    *dot = '\0';

				printf("tree:\n");
				tree.print(fout, false);
				printf("\n--------------\n");
                fclose(fout);
            }
            else
            {   printf("Cannot open: %s\n", file_name);
                return 0;
            }
        }
        else if (!strcmp(arg, "-pc"))
        {   
            printf("tree:\n");
            tree.print(stdout, true);
            printf("\n--------------\n");
        }
        else if (!strcmp(arg, "-o") && i + 1 < argc)
        {
            char *file_name = argv[++i];
            FILE *fout = !strcmp(file_name, "-") 
                         ? stdout : fopen(file_name, "wt");

            if (fout != 0)
            {   char *dot = strstr(file_name, ".");
                if (dot)
                    *dot = '\0';

                print_tree_to_c(fout, tree, file_name);
                fclose(fout);
            }
            else
            {   printf("Cannot open: %s\n", file_name);
                return 0;
            }
        }
        else if (!strcmp(arg, "-xml") && i + 1 < argc)
        {
            char *file_name = argv[++i];
            FILE *fout = !strcmp(file_name, "-") 
                         ? stdout : fopen(file_name, "w");

            if (fout != 0)
            {   char *dot = strstr(file_name, ".");
                if (dot)
                    *dot = '\0';

                print_tree_to_xml(fout, tree);
                fclose(fout);
            }
            else
            {   printf("Cannot open: %s\n", file_name);
                return 0;
            }
        }
		else if (!strcmp(arg, "-unparse") && i + 1 < argc)
		{
			char *file_name = argv[i+1];
			FILE *fout = !strcmp(file_name, "-") 
						 ? stdout : fopen(file_name, "wb");
			if (fout == 0)
				fprintf(stderr, "Error: cannot open %s for writing\n", file_name);
			else
			{
				CharStreamToFile charToTextFileStream(fout, /*text*/true);
				CharStreamToFile charToFileStream(fout, /*text*/false);
				BasicTerminalUnparser basicTerminalUnparser;
				WhiteSpaceTerminalUnparser whiteSpaceTerminalUnparser;
				ResourceTerminalUnparser resourceTerminalUnparser;
				Unparser unparser;
				if (use_scanner == constResource)
					unparser.setTerminalUnparser(&resourceTerminalUnparser);
				else if (use_scanner = constWhiteSpace)
					unparser.setTerminalUnparser(&whiteSpaceTerminalUnparser);
				else
					unparser.setTerminalUnparser(&basicTerminalUnparser);
				class UnparseErrorCollector : public AbstractUnparseErrorCollector
				{
				public:
					UnparseErrorCollector(FILE *f) : _f(f) {}
					virtual void errorDifferentRulesWithSameType(Ident type, GrammarRule* rule1, GrammarRule* rule2)
					{
						fprintf(_f, "Error: different rules with type %s\n", type.val());
						print_rule(rule1);
						print_rule(rule2);
					}
					virtual void warningTypeReachedThroughDifferentPaths(Ident type, GrammarRule* rule, GrammarRule *rule1, GrammarRule *rule2)
					{
						fprintf(_f, "Warning: rule for type %s reached through different paths\n", type.val());
						print_rule(rule);
						print_rule(rule1);
						print_rule(rule2);
					}
				private:
					void print_rule(GrammarRule* rule)
					{
						if (rule != 0)
						{
							fprintf(_f, "\t%d.%d ", rule->line, rule->column);
							rule->print(_f);
							fprintf(_f, "\n");
						}
						else
							fprintf(_f, "\t<empty rule>\n");
					}
					FILE *_f;
				};
				UnparseErrorCollector unparseErrorCollector(stderr);
				unparser.loadGrammarForUnparse(grammarTree, &unparseErrorCollector);
				AbstractStream<char> *outStream;
				if (selected_output_converter == 0)
					outStream = &charToTextFileStream;
				else
				{
					selected_output_converter->setOutputStream(&charToFileStream);
					outStream = selected_output_converter;
				}
				unparser.unparse(tree, "root", outStream);

				if (strcmp(file_name, "-") != 0)
					fclose(fout);
			}
			i++;
		}
        else if (!strcmp(arg, "+ds"))
            debug_scan = true;
        //else if (!strcmp(arg, "+dss"))
        //   debug_scan = 1;
        else if (!strcmp(arg, "-ds"))
            debug_scan = false;
        else if (!strcmp(arg, "+dp"))
            debug_parse = true;
        else if (!strcmp(arg, "-dp"))
            debug_parse = false;
        else if (!strcmp(arg, "+dn"))
            debug_nt = true;
        else if (!strcmp(arg, "-dn"))
            debug_nt = false;
        else
        {   const char* filename = arg;
        	FILE *fin = fopen(filename, "rt");

            if (fin != 0)
            {
				TextFileBuffer textBuffer;
				selected_file_reader->read(fin, textBuffer);

				AbstractParser* parser =   selected_parser == constBTHeap 
					                     ? (AbstractParser*)new BTHeapParser()
										 : selected_parser == constBTStack
										 ? (AbstractParser*)new BTParser()
										 : selected_parser == constLL1Heap 
					                     ? (AbstractParser*)new LL1HeapParser()
										 : selected_parser == constLL1Stack 
					                     ? (AbstractParser*)new LL1Parser()
										 : (AbstractParser*)new ParParser();
				AbstractScanner* scanner = use_scanner == constWhiteSpace
										 ? (AbstractScanner*)new WhiteSpaceScanner()
										 : use_scanner == constProtos
										 ? (AbstractScanner*)new ProtosScanner()
										 : use_scanner == constResource
										 ? (AbstractScanner*)new ResourceScanner()
										 : use_scanner == constPascal
										 ? (AbstractScanner*)new PascalScanner()
										 : use_scanner == constColourCoding
										 ? (AbstractScanner*)new ColourCodingScanner()
										 : use_scanner == constRaw
										 ? (AbstractScanner*)new RawScanner()
										 : use_scanner == constBare
										 ? (AbstractScanner*)new BareScanner()
										 : (AbstractScanner*)new BasicScanner();
				parser->setScanner(scanner);
				parser->setDebugLevel(debug_nt, debug_parse, debug_scan);
				parser->loadGrammar(tree);
				grammarTree = tree;

				AbstractParseTree new_tree;
				if (!parser->parse(textBuffer, "root", new_tree))
				{
					parser->printExpected(stdout, filename, textBuffer);
					return 0;
				}
				textBuffer.release();

               	tree.attach(new_tree);

                fclose(fin);
            }
            else
            {   printf("Cannot open: %s\n", arg);
                return 0;
            }
        }
    }

	tree.clear();

    return 0;
}
