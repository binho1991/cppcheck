/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2013 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tokenize.h"
#include "checknullpointer.h"
#include "testsuite.h"
#include <sstream>

extern std::ostringstream errout;

class TestNullPointer : public TestFixture {
public:
    TestNullPointer() : TestFixture("TestNullPointer") {
    }

private:


    void run() {
        TEST_CASE(nullpointerAfterLoop);
        TEST_CASE(nullpointer1);
        TEST_CASE(nullpointer2);
        TEST_CASE(structDerefAndCheck);    // dereferencing struct and then checking if it's null
        TEST_CASE(pointerDerefAndCheck);
        TEST_CASE(nullpointer5);    // References should not be checked
        TEST_CASE(nullpointerExecutionPaths);
        TEST_CASE(nullpointerExecutionPathsLoop);
        TEST_CASE(nullpointer7);
        TEST_CASE(nullpointer8);
        TEST_CASE(nullpointer9);
        TEST_CASE(nullpointer10);
        TEST_CASE(nullpointer11); // ticket #2812
        TEST_CASE(nullpointer12); // ticket #2470
        TEST_CASE(nullpointer14);
        TEST_CASE(nullpointer15); // #3560 (fp: return p ? f(*p) : f(0))
        TEST_CASE(nullpointer16); // #3591
        TEST_CASE(nullpointer17); // #3567
        TEST_CASE(nullpointer18); // #1927
        TEST_CASE(nullpointer19); // #3811
        TEST_CASE(nullpointer20); // #3807 (fp: return p ? (p->x() || p->y()) : z)
        TEST_CASE(nullpointer21); // #4038 (fp: if (x) p=q; else return;)
        TEST_CASE(nullpointer23); // #4665 (false positive)
        TEST_CASE(nullpointer24); // #5082 fp: chained assignment
        TEST_CASE(nullpointer_cast); // #4692
        TEST_CASE(nullpointer_castToVoid); // #3771
        TEST_CASE(pointerCheckAndDeRef);     // check if pointer is null and then dereference it
        TEST_CASE(nullConstantDereference);  // Dereference NULL constant
        TEST_CASE(gcc_statement_expression); // Don't crash
        TEST_CASE(snprintf_with_zero_size);
        TEST_CASE(snprintf_with_non_zero_size);
        TEST_CASE(printf_with_invalid_va_argument);
        TEST_CASE(scanf_with_invalid_va_argument);
        TEST_CASE(nullpointer_in_return);
        TEST_CASE(nullpointer_in_typeid);
        TEST_CASE(nullpointer_in_for_loop);
        TEST_CASE(nullpointerDelete);
        TEST_CASE(nullpointerExit);
        TEST_CASE(nullpointerStdString);
        TEST_CASE(nullpointerStdStream);
        TEST_CASE(functioncall);
        TEST_CASE(functioncalllibrary); // use Library to parse function call
        TEST_CASE(crash1);
        TEST_CASE(functioncallDefaultArguments);
        TEST_CASE(nullpointer_internal_error); // #5080
    }

    void check(const char code[], bool inconclusive = false, const char filename[] = "test.cpp", bool verify=true) {
        // Clear the error buffer..
        errout.str("");

        Settings settings;
        settings.addEnabled("warning");
        settings.inconclusive = inconclusive;

        // cfg
        const char cfg[] = "<?xml version=\"1.0\"?>\n"
                           "<def>\n"
                           "  <function name=\"strcpy\"> <arg nr=\"1\"><not-null/></arg> </function>\n"
                           "</def>";
        settings.library.loadxmldata(cfg, sizeof(cfg));

        // Tokenize..
        Tokenizer tokenizer(&settings, this);
        std::istringstream istr(code);
        if (!tokenizer.tokenize(istr, filename))
            return;

        // Check for redundant code..
        CheckNullPointer checkNullPointer(&tokenizer, &settings, this);
        checkNullPointer.nullPointer();

        const std::string str1(tokenizer.tokens()->stringifyList(0,true));
        tokenizer.simplifyTokenList2();
        const std::string str2(tokenizer.tokens()->stringifyList(0,true));
        if (verify && str1 != str2)
            warn(("Unsimplified code in test case. It looks like this test "
                  "should either be cleaned up or moved to TestTokenizer or "
                  "TestSimplifyTokens instead.\nstr1="+str1+"\nstr2="+str2).c_str());

        checkNullPointer.nullConstantDereference();
        checkNullPointer.executionPaths();
    }


    void nullpointerAfterLoop() {
        check("int foo(const Token *tok)\n"
              "{\n"
              "    while (tok);\n"
              "    tok = tok->next();\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:3]: (warning) Possible null pointer dereference: tok - otherwise it is redundant to check it against null.\n", errout.str());

        // #2681
        {
            const char code[] = "void foo(const Token *tok)\n"
                                "{\n"
                                "    while (tok && tok->str() == \"=\")\n"
                                "        tok = tok->next();\n"
                                "\n"
                                "    if (tok->str() != \";\")\n"
                                "        ;\n"
                                "}\n";

            check(code, false);   // inconclusive=false => no error
            ASSERT_EQUALS("", errout.str());

            check(code, true);    // inconclusive=true => error
            ASSERT_EQUALS("[test.cpp:6] -> [test.cpp:3]: (warning, inconclusive) Possible null pointer dereference: tok - otherwise it is redundant to check it against null.\n", errout.str());
        }

        check("void foo()\n"
              "{\n"
              "    for (const Token *tok = tokens; tok; tok = tok->next())\n"
              "    {\n"
              "        while (tok && tok->str() != \";\")\n"
              "            tok = tok->next();\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:5]: (warning) Possible null pointer dereference: tok - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(Token &tok)\n"
              "{\n"
              "    for (int i = 0; i < tok.size(); i++ )\n"
              "    {\n"
              "        while (!tok)\n"
              "            char c = tok.read();\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo()\n"
              "{\n"
              "    for (const Token *tok = tokens; tok; tok = tok->next())\n"
              "    {\n"
              "        while (tok && tok->str() != \";\")\n"
              "            tok = tok->next();\n"
              "        if( !tok ) break;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo()\n"
              "{\n"
              "    for (const Token *tok = tokens; tok; tok = tok ? tok->next() : NULL)\n"
              "    {\n"
              "        while (tok && tok->str() != \";\")\n"
              "            tok = tok->next();\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(A*a)\n"
              "{\n"
              "  switch (a->b()) {\n"
              "    case 1:\n"
              "      while( a ){\n"
              "        a = a->next;\n"
              "      }\n"
              "    break;\n"
              "    case 2:\n"
              "      a->b();\n"
              "      break;\n"
              "  }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // dereference in outer scope..
        {
            const char code[] = "void foo(int x, const Token *tok) {\n"
                                "    if (x == 123) {\n"
                                "        while (tok) tok = tok->next();\n"
                                "    }\n"
                                "    tok->str();\n"
                                "}\n";

            check(code, false);
            ASSERT_EQUALS("", errout.str());

            check(code, true);
            ASSERT_EQUALS("[test.cpp:5] -> [test.cpp:3]: (warning, inconclusive) Possible null pointer dereference: tok - otherwise it is redundant to check it against null.\n", errout.str());
        }

        check("int foo(const Token *tok)\n"
              "{\n"
              "    while (tok){;}\n"
              "}\n", true);
        ASSERT_EQUALS("", errout.str());

        check("int foo(const Token *tok)\n"
              "{\n"
              "    while (tok){;}\n"
              "    char a[2] = {0,0};\n"
              "}\n", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer1() {
        // ticket #1923 - no false positive when using else if
        check("void f(A *a)\n"
              "{\n"
              "    if (a->x == 1)\n"
              "    {\n"
              "        a = a->next;\n"
              "    }\n"
              "    else if (a->x == 2) { }\n"
              "    if (a) { }\n"
              "}", false, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        // ticket #2134 - sizeof doesn't dereference
        check("void f() {\n"
              "    int c = 1;\n"
              "    int *list = NULL;\n"
              "    sizeof(*list);\n"
              "    if (!list)\n"
              "        ;\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        // ticket #2245 - sizeof doesn't dereference
        check("void f(Bar *p) {\n"
              "    if (!p) {\n"
              "        int sz = sizeof(p->x);\n"
              "    }\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

    }

    void nullpointer2() {
        // Null pointer dereference can only happen with pointers
        check("void foo()\n"
              "{\n"
              "    Fred fred;\n"
              "    while (fred);\n"
              "    fred.hello();\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    // Dereferencing a struct and then checking if it is null
    // This is checked by this function:
    //        CheckOther::nullPointerStructByDeRefAndChec
    void structDerefAndCheck() {
        // errors..
        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    int a = abc->a;\n"
              "    if (!abc)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(struct ABC *abc) {\n"
              "    bar(abc->a);\n"
              "    if (!abc)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:3]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(ABC *abc) {\n"
              "    if (abc->a == 3) {\n"
              "        return;\n"
              "    }\n"
              "    if (abc) {}\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:5]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

        check("void f(ABC *abc) {\n"
              "    if (abc->x == 0) {\n"
              "        return;\n"
              "    }\n"
              "    if (!abc);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:5]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

        // TODO: False negative if member of member is dereferenced
        check("void foo(ABC *abc) {\n"
              "    abc->next->a = 0;\n"
              "    if (abc->next)\n"
              "        ;\n"
              "}");
        TODO_ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:3]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", "", errout.str());

        check("void foo(ABC *abc) {\n"
              "    abc->a = 0;\n"
              "    if (abc && abc->b == 0)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:3]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

        // ok dereferencing in a condition
        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    if (abc && abc->a);\n"
              "    if (!abc)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(struct ABC *abc) {\n"
              "    int x = abc && a(abc->x);\n"
              "    if (abc) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // ok to use a linked list..
        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    abc = abc->next;\n"
              "    if (!abc)\n"
              "        ;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void f(struct ABC *abc) {\n"
              "    abc = (ABC *)(abc->_next);\n"
              "    if (abc) { }"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        // reassign struct..
        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    int a = abc->a;\n"
              "    abc = abc->next;\n"
              "    if (!abc)\n"
              "        ;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    int a = abc->a;\n"
              "    f(&abc);\n"
              "    if (!abc)\n"
              "        ;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // goto..
        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    int a;\n"
              "    if (!abc)\n"
              "        goto out;"
              "    a = abc->a;\n"
              "    return;\n"
              "out:\n"
              "    if (!abc)\n"
              "        ;\n"
              "}",false,"test.cpp",false);
        ASSERT_EQUALS("", errout.str());

        // loops..
        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    int a = abc->a;"
              "    do\n"
              "    {\n"
              "        if (abc)\n"
              "            abc = abc->next;\n"
              "        --a;\n"
              "    }\n"
              "    while (a > 0);\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f()\n"
              "{\n"
              "    for (const Token *tok = _tokenizer->tokens(); tok; tok = tok->next())\n"
              "    {\n"
              "        while (tok && tok->str() != \"{\")\n"
              "            tok = tok->next();\n"
              "        if (!tok)\n"
              "            return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // dynamic_cast..
        check("void foo(ABC *abc)\n"
              "{\n"
              "    int a = abc->a;\n"
              "    if (!dynamic_cast<DEF *>(abc))\n"
              "        ;\n"
              "}",false,"test.cpp",false);
        ASSERT_EQUALS("", errout.str());

        // #2641 - global pointer, function call
        check("ABC *abc;\n"
              "void f() {\n"
              "    abc->a = 0;\n"
              "    do_stuff();\n"
              "    if (abc) { }\n"
              "}");
        ASSERT_EQUALS("",errout.str());

        check("Fred *fred;\n"
              "void f() {\n"
              "    fred->foo();\n"
              "    if (fred) { }\n"
              "}");
        ASSERT_EQUALS("",errout.str());

        // #2641 - local pointer, function call
        check("void f() {\n"
              "    ABC *abc;\n"
              "    abc->a = 0;\n"
              "    do_stuff();\n"
              "    if (abc) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:5]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n",errout.str());

        // #2641 - local pointer, function call
        check("void f(ABC *abc) {\n"
              "    abc->a = 0;\n"
              "    do_stuff();\n"
              "    if (abc) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:4]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n",errout.str());

        // #2691 - switch/break
        check("void f(ABC *abc) {\n"
              "    switch ( x ) {\n"
              "        case 14:\n"
              "            sprintf(buf, \"%d\", abc->a);\n"
              "            break;\n"
              "        case 15:\n"
              "            if ( abc ) {}\n"
              "            break;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #3128
        check("void f(ABC *abc) {\n"
              "    x(!abc || y(abc->a));\n"
              "    if (abc) {}\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(ABC *abc) {\n"
              "  x(def || !abc || y(def, abc->a));\n"
              "  if (abc) {}\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(ABC *abc) {\n"
              "  x(abc && y(def, abc->a));\n"
              "  if (abc) {}\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(ABC *abc) {\n"
              "    x(def && abc && y(def, abc->a));\n"
              "    if (abc) {}\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #3228 - calling function with null object
        {
            const char code[] = "void f(Fred *fred) {\n"
                                "    fred->x();\n"
                                "    if (fred) { }\n"
                                "}";
            check(code);
            ASSERT_EQUALS("", errout.str());
            check(code, true);
            ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:3]: (warning, inconclusive) Possible null pointer dereference: fred - otherwise it is redundant to check it against null.\n", errout.str());
        }

        // #3425 - false positives when there are macros
        check("void f(struct FRED *fred) {\n"
              "    fred->x = 0;\n"
              "    $if(!fred){}\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    // Dereferencing a pointer and then checking if it is null
    void pointerDerefAndCheck() {
        // errors..
        check("void foo(int *p)\n"
              "{\n"
              "    *p = 0;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(int *p)\n"
              "{\n"
              "    *p = 0;\n"
              "    if (p) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(int *p)\n"
              "{\n"
              "    *p = 0;\n"
              "    if (p || q) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(int *p)\n"
              "{\n"
              "    bar(*p);\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p)\n"
              "{\n"
              "    strcpy(p, \"abc\");\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p)\n"
              "{\n"
              "    if (*p == 0) { }\n"
              "    if (!p) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // no error
        check("void foo()\n"
              "{\n"
              "    int *p;\n"
              "    f(&p);\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo()\n"
              "{\n"
              "    int **p = f();\n"
              "    if (!p)\n"
              "        ;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void foo(int *p)\n"
              "{\n"
              "    if (x)\n"
              "        p = 0;\n"
              "    else\n"
              "        *p = 0;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(int x)\n"
              "{\n"
              "    int a = 2 * x;"
              "    if (x == 0)\n"
              "        ;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void foo(int *p)\n"
              "{\n"
              "    int var1 = p ? *p : 0;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(int *p)\n"
              "{\n"
              "    int var1 = x ? *p : 5;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        TODO_ASSERT_EQUALS("error", "", errout.str());

        // while
        check("void f(int *p) {\n"
              "    *p = 0;\n"
              "    while (p) { p = 0; }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p) {\n"
              "    *p = 0;\n"
              "    while (p) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:3]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // Ticket #3125
        check("void foo(ABC *p)\n"
              "{\n"
              "    int var1 = p ? (p->a) : 0;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(ABC *p)\n"
              "{\n"
              "    int var1 = p ? (1 + p->a) : 0;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "    int * a=0;\n"
              "    if (!a) {};\n"
              "    int c = a ? 0 : 1;\n"
              "}\n",true,"test.cpp",false);
        ASSERT_EQUALS("", errout.str());

        // #3686
        check("void f() {\n"
              "    int * a=0;\n"
              "    if (!a) {};\n"
              "    int c = a ? b : b+1;\n"
              "}\n",true,"test.cpp",false);
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "    int * a=0;\n"
              "    if (!a) {};\n"
              "    int c = (a) ? b : b+1;\n"
              "}\n",true,"test.cpp",false);
        ASSERT_EQUALS("", errout.str());

        check("void foo(P *p)\n"
              "{\n"
              "  while (p)\n"
              "    if (p->check())\n"
              "      break;\n"
              "    else\n"
              "      p = p->next();\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(Document *doc) {\n"
              "    int x = doc && doc->x;\n"
              "    if (!doc) {\n"
              "        return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #3128 - false positive
        check("void f(int *p) {\n"
              "    assert(!p || (*p<=6));\n"
              "    if (p) { *p = 0; }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p) {\n"
              "    assert(p && (*p<=6));\n"
              "    if (p) { *p = 0; }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p) {\n"
              "    *p = 12;\n"
              "    assert(p && (*p<=6));\n"
              "    if (p) { *p = 0; }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:4]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(x *p)\n"
              "{\n"
              "    p = p->next;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(x *p)\n"
              "{\n"
              "    p = bar(p->next);\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(x *p)\n"
              "{\n"
              "    p = aa->bar(p->next);\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(x *p)\n"
              "{\n"
              "    p = *p2 = p->next;\n"
              "    if (!p)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(struct ABC *abc)\n"
              "{\n"
              "    abc = abc ? abc->next : 0;\n"
              "    if (!abc)\n"
              "        ;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(struct ABC *abc) {\n" // #4523
              "    abc = (*abc).next;\n"
              "    if (abc) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(struct ABC *abc) {\n" // #4523
              "    abc = (*abc->ptr);\n"
              "    if (abc) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("int f(Item *item) {\n"
              "    x = item ? ab(item->x) : 0;\n"
              "    if (item) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("int f(Item *item) {\n"
              "    item->x = 0;\n"
              "    a = b ? c : d;\n"
              "    if (item) { }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:4]: (warning) Possible null pointer dereference: item - otherwise it is redundant to check it against null.\n", errout.str());

        check("BOOL GotoFlyAnchor()\n"  // #2243
              "{\n"
              "    const SwFrm* pFrm = GetCurrFrm();\n"
              "    do {\n"
              "        pFrm = pFrm->GetUpper();\n"
              "    } while( pFrm && !pFrm->IsFlyFrm() );\n"
              "\n"
              "    if( !pFrm )\n"
              "        return FALSE;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // Ticket #2463
        check("struct A\n"
              "{\n"
              "    B* W;\n"
              "\n"
              "    void f() {\n"
              "        switch (InData) {\n"
              "            case 2:\n"
              "                if (!W) return;\n"
              "                W->foo();\n"
              "                break;\n"
              "            case 3:\n"
              "                f();\n"
              "                if (!W) return;\n"
              "                break;\n"
              "        }\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #2525 - sizeof
        check("void f() {\n"
              "    int *test = NULL;\n"
              "    int c = sizeof(test[0]);\n"
              "    if (!test)\n"
              "        ;\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        check("void f(type* p) {\n" // #4983
              "    x(sizeof p[0]);\n"
              "    if (!p)\n"
              "        ;\n"
              "}", false, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        // #3023 - checked deref
        check("void f(struct ABC *abc) {\n"
              "  WARN_ON(!abc || abc->x == 0);\n"
              "  if (!abc) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());
        check("void f(struct ABC *abc) {\n"
              "  WARN_ON(!abc || abc->x == 7);\n"
              "  if (!abc) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #3425 - false positives when there are macros
        check("void f(int *p) {\n"
              "    *p = 0;\n"
              "    $if(!p){}\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n" // #3914 - false positive
              "    int *p;\n"
              "    ((p=ret()) && (x=*p));\n"
              "    if (p);\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer5() {
        // errors..
        check("void foo(A &a)\n"
              "{\n"
              " char c = a.c();\n"
              " if (!a)\n"
              "   return;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    // Execution paths..
    void nullpointerExecutionPaths() {
        // errors..
        check("static void foo()\n"
              "{\n"
              "    Foo *p = 0;\n"
              "    if (a == 1) {\n"
              "        p = new FooBar;\n"
              "    } else { if (a == 2) {\n"
              "        p = new FooCar; } }\n"
              "    p->abcd();\n"
              "}");
        TODO_ASSERT_EQUALS("[test.cpp:8]: (error) Possible null pointer dereference: p\n",
                           "", errout.str());

        check("static void foo() {\n"
              "    int &r = *0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("static void foo(int x) {\n"
              "    int y = 5 + *0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        {
            const char code[] = "static void foo(int x)\n"
                                "{\n"
                                "    Foo<int> *abc = 0;\n"
                                "    abc->a();\n"
                                "}\n";

            // inconclusive=false => no error
            check(code,false);
            ASSERT_EQUALS("", errout.str());

            // inconclusive=true => error
            check(code, true);
            ASSERT_EQUALS("[test.cpp:4]: (error) Possible null pointer dereference: abc\n", errout.str());
        }

        check("static void foo() {\n"
              "    std::cout << *0;"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f()\n"
              "{\n"
              "    char *c = 0;\n"
              "    {\n"
              "        delete c;\n"
              "    }\n"
              "    c[0] = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:7]: (error) Possible null pointer dereference: c\n", errout.str());

        check("static void foo() {\n"
              "    if (3 > *0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        // no false positive..
        check("static void foo()\n"
              "{\n"
              "    Foo *p = 0;\n"
              "    p = new Foo;\n"
              "    p->abcd();\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo()\n"
              "{\n"
              "    int sz = sizeof((*(struct dummy *)0).x);\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void get_offset(long &offset)\n"
              "{\n"
              "    mystruct * temp; temp = 0;\n"
              "    offset = (long)(&(temp->z));\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // Ticket #1893 - try/catch inside else
        check("int *test(int *Z)\n"
              "{\n"
              "    int *Q=NULL;\n"
              "    if (Z) {\n"
              "        Q = Z;\n"
              "    }\n"
              "    else {\n"
              "        Z = new int;\n"
              "        try {\n"
              "        } catch(...) {\n"
              "        }\n"
              "        Q = Z;\n"
              "    }\n"
              "    *Q=1;\n"
              "    return Q;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("int *test(int *Z)\n"
              "{\n"
              "    int *Q=NULL;\n"
              "    if (Z) {\n"
              "        Q = Z;\n"
              "    }\n"
              "    else {\n"
              "        try {\n"
              "        } catch(...) {\n"
              "        }\n"
              "    }\n"
              "    *Q=1;\n"
              "    return Q;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:12]: (error) Possible null pointer dereference: Q\n", errout.str());

        // Ticket #2052 (false positive for 'else continue;')
        check("void f() {\n"
              "    for (int x = 0; x < 5; ++x) {"
              "        int *p = 0;\n"
              "        if (a(x)) p=b(x);\n"
              "        else continue;\n"
              "        *p = 0;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // function pointer..
        check("void foo()\n"
              "{\n"
              "    void (*f)();\n"
              "    f = 0;\n"
              "    f();\n"
              "}");
        ASSERT_EQUALS("[test.cpp:5]: (error) Possible null pointer dereference: f\n", errout.str());

        // loops..
        check("void f() {\n"
              "    int *p = 0;\n"
              "    for (int i = 0; i < 10; ++i) {\n"
              "        int x = *p + 1;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4]: (error) Possible null pointer dereference: p\n", errout.str());

        check("void f(int a) {\n"
              "    const char *p = 0;\n"
              "    if (a) {\n"
              "        p = \"abcd\";\n"
              "    }\n"
              "    for (int i = 0; i < 3; i++) {\n"
              "        if (a && (p[i] == '1'));\n"
              "    }\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // ticket #2251: taking the address of member
        check("void f() {\n"
              "    Fred *fred = 0;\n"
              "    int x = &fred->x;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // ticket #3220: calling member function
        check("void f() {\n"
              "    Fred *fred = NULL;\n"
              "    fred->do_something();\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // ticket #3570 - parsing of conditions
        {
            check("void f() {\n"
                  "    int *p = NULL;\n"
                  "    if (x)\n"
                  "        p = q;\n"
                  "    if (p && *p) { }\n"
                  "}", true);
            ASSERT_EQUALS("", errout.str());
            check("void f() {\n"
                  "    int *p = NULL;\n"
                  "    if (x)\n"
                  "        p = q;\n"
                  "    if (!p || *p) { }\n"
                  "}", true);
            ASSERT_EQUALS("", errout.str());
            check("void f() {\n"
                  "    int *p = NULL;\n"
                  "    if (x)\n"
                  "        p = q;\n"
                  "    if (p || *p) { }\n"
                  "}");
            TODO_ASSERT_EQUALS("error", "", errout.str());
        }
    }

    // Ticket #2350
    void nullpointerExecutionPathsLoop() {
        // No false positive:
        check("void foo() {\n"
              "    int n;\n"
              "    int *argv32;\n"
              "    if (x) {\n"
              "        n = 0;\n"
              "        argv32 = 0;\n"
              "    }\n"
              "\n"
              "    for (int i = 0; i < n; i++) {\n"
              "        argv32[i] = 0;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // No false negative:
        check("void foo() {\n"
              "    int n;\n"
              "    int *argv32;\n"
              "    if (x) {\n"
              "        n = 10;\n"
              "        argv32 = 0;\n"
              "    }\n"
              "\n"
              "    for (int i = 0; i < n; i++) {\n"
              "        argv32[i] = 0;\n"
              "    }\n"
              "}");
        TODO_ASSERT_EQUALS("error",
                           "", errout.str());

        // #2231 - error if assignment in loop is not used
        check("void f() {\n"
              "    char *p = 0;\n"
              "\n"
              "    for (int x = 0; x < 3; ++x) {\n"
              "        if (y[x] == 0) {\n"
              "            p = malloc(10);\n"
              "            break;\n"
              "        }\n"
              "    }\n"
              "\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:11]: (error) Possible null pointer dereference: p\n", errout.str());
    }

    void nullpointer7() {
        check("void foo()\n"
              "{\n"
              "  wxLongLong x = 0;\n"
              "  int y = x.GetValue();\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer8() {
        check("void foo()\n"
              "{\n"
              "  char const * x = 0;\n"
              "  strdup(x);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4]: (error) Possible null pointer dereference: x\n", errout.str());
    }

    void nullpointer9() { //#ticket 1778
        check("void foo()\n"
              "{\n"
              "  std::string * x = 0;\n"
              "  *x = \"test\";\n"
              "}", false, "test.cpp", false);
        ASSERT_EQUALS("[test.cpp:4]: (error) Null pointer dereference\n", errout.str());
    }

    void nullpointer10() {
        check("void foo()\n"
              "{\n"
              "  struct my_type* p = 0;\n"
              "  p->x = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4]: (error) Possible null pointer dereference: p\n", errout.str());
    }

    void nullpointer11() { // ticket #2812
        check("int foo()\n"
              "{\n"
              "  struct my_type* p;\n"
              "  p = 0;\n"
              "  return p->x;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:5]: (error) Possible null pointer dereference: p\n", errout.str());
    }

    void nullpointer12() { // ticket #2470, #4035
        const char code[] = "int foo()\n"
                            "{\n"
                            "  int* i = nullptr;\n"
                            "  return *i;\n"
                            "}\n";

        check(code, false, "test.cpp", false); // C++ file => nullptr means NULL
        ASSERT_EQUALS("[test.cpp:4]: (error) Null pointer dereference\n", errout.str());

        check(code, false, "test.c", false); // C file => nullptr does not mean NULL
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer14() {
        check("void foo()\n"
              "{\n"
              "  strcpy(bar, 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Null pointer dereference\n", errout.str());

        check("void foo()\n"
              "{\n"
              "  memcmp(bar(xyz()), 0, 123);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Null pointer dereference\n", errout.str());

        check("void foo(const char *s)\n"
              "{\n"
              "    char *p = malloc(100);\n"
              "    frexp(1.0, p);\n"
              "    char *q = 0;\n"
              "    frexp(1.0, q);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:6]: (error) Possible null pointer dereference: q\n", errout.str());
    }

    void nullpointer15() {  // #3560
        check("void f() {\n"
              "    char *p = 0;\n"
              "    if (x) p = \"abcd\";\n"
              "    return p ? f(*p) : f(0);\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer16() {  // #3591
        check("void foo() {\n"
              "    int *p = 0;\n"
              "    bar(&p);\n"
              "    *p = 0;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer17() {  // #3567
        check("int foo() {\n"
              "    int *p = 0;\n"
              "    if (x) { return 0; }\n"
              "    return !p || *p;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("int foo() {\n"
              "    int *p = 0;\n"
              "    if (x) { return 0; }\n"
              "    return p && *p;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer18() {  // #1927
        check("void f ()\n"
              "{\n"
              "  int i=0;\n"
              "  char *str=NULL;\n"
              "  while (str[i])\n"
              "  {\n"
              "    i++;\n"
              "  };\n"
              "}\n", false, "test.cpp", false);
        ASSERT_EQUALS("[test.cpp:5]: (error) Null pointer dereference\n", errout.str());
    }

    void nullpointer19() { // #3811
        check("int foo() {\n"
              "    perror(0);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer20() {  // #3807
        check("void f(int x) {\n"
              "    struct xy *p = 0;\n"
              "    if (x) p = q;\n"
              "    if (p ? p->x || p->y : 0) { }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int x) {\n"   // false negative
              "    struct xy *p = 0;\n"
              "    if (x) p = q;\n"
              "    if (y ? p->x : p->y) { }\n"
              "}");
        TODO_ASSERT_EQUALS("error", "", errout.str());
    }

    void nullpointer21() {  // #4038 - fp: if (x) p=q; else return;
        check("void f(int x) {\n"
              "    int *p = 0;\n"
              "    if (x) p = q;\n"
              "    else return;\n"
              "    *p = 0;\n" // <- p is not NULL
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer23() {  // #4665
        check("void f(){\n"
              "    char *c = NULL;\n"
              "    char cBuf[10];\n"
              "    sprintf(cBuf, \"%s\", c ? c : \"0\" );\n"
              "}");
        ASSERT_EQUALS("",errout.str());
    }

    void nullpointer24() {  // #5083 - fp: chained assignment
        check("void f(){\n"
              "    char *c = NULL;\n"
              "    x = c = new char[10];\n"
              "    *c = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer_cast() { // #4692
        check("char *nasm_skip_spaces(const char *p) {\n"
              "    if (p)\n"
              "        while (*p && nasm_isspace(*p))\n"
              "            p++;\n"
              "    return p;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer_castToVoid() {  // #3771
        check("void f () {\n"
              "    int *buf; buf = NULL;\n"
              "    buf;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    // Check if pointer is null and the dereference it
    void pointerCheckAndDeRef() {
        check("void foo(char *p) {\n"
              "    if (!p) {\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (NULL == p) {\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (p == NULL) {\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (p == NULL) {\n"
              "    }\n"
              "    printf(\"%c\", *p);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (p && *p == 0) {\n"
              "    }\n"
              "    printf(\"%c\", *p);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (p && *p == 0) {\n"
              "    } else { *p = 0; }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (p) {\n"
              "    }\n"
              "    strcpy(p, \"abc\");\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(char *p) {\n"
              "    if (p) {\n"
              "    }\n"
              "    bar();\n"
              "    strcpy(p, \"abc\");\n"
              "}");
        ASSERT_EQUALS("[test.cpp:5] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void foo(abc *p) {\n"
              "    if (!p) {\n"
              "    }\n"
              "    else { if (!p->x) {\n"
              "    } }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        {
            static const char code[] =
                "void foo(char *p) {\n"
                "    if (!p) {\n"
                "        abort();\n"
                "    }\n"
                "    *p = 0;\n"
                "}";
            check(code, false);
            ASSERT_EQUALS("", errout.str());

            check(code, true);
            ASSERT_EQUALS("[test.cpp:5] -> [test.cpp:2]: (warning, inconclusive) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());
        }

        check("void foo(char *p) {\n"
              "    if (!p) {\n"
              "        (*bail)();\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(char *p) {\n"
              "    if (!p) {\n"
              "        throw x;\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(char *p) {\n"
              "    if (!p) {\n"
              "        ab.abort();\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void foo(char *p) {\n"
              "    if (!p) {\n"
              "        switch (x) { }\n"
              "    }\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void foo(char *p) {\n"
              "    if (!p) {\n"
              "    }\n"
              "    return *x;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("int foo(int *p) {\n"
              "    if (!p) {\n"
              "        x = *p;\n"
              "        return 5+*p;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n"
                      "[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // operator!
        check("void f() {\n"
              "    A a;\n"
              "    if (!a) {\n"
              "        a.x();\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // This is why this check can't be used on the simplified token list
        check("void f(Foo *foo) {\n"
              "    if (!dynamic_cast<bar *>(foo)) {\n"
              "        *foo = 0;\n"
              "    }\n"
              "}", false, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        // ticket: #2300 - calling unknown function that may initialize the pointer
        check("Fred *fred;\n"
              "void a() {\n"
              "    if (!fred) {\n"
              "        initfred();\n"
              "        fred->x = 0;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // ticket #1219
        check("void foo(char *p) {\n"
              "    if (p) {\n"
              "        return;\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:5] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // #2467 - unknown macro may terminate the application
        check("void f(Fred *fred) {\n"
              "    if (fred == NULL) {\n"
              "        MACRO;\n"
              "    }\n"
              "    fred->a();\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #2493 - switch
        check("void f(Fred *fred) {\n"
              "    if (fred == NULL) {\n"
              "        x = 0;\n"
              "    }\n"
              "    switch (x) {\n"
              "        case 1:\n"
              "            fred->a();\n"
              "            break;\n"
              "    };\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #4118 - second if
        check("void f(char *p) {\n"
              "    int x = 1;\n"
              "    if (!p) x = 0;\n"
              "    if (x) *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // #2582 - segmentation fault
        check("if()");

        // #2674 - different functions
        check("class Fred {\n"
              "public:\n"
              "    Wilma *wilma;\n"
              "    void a();\n"
              "    void b();\n"
              "};\n"
              "\n"
              "void Fred::a() {\n"
              "    if ( wilma ) { }\n"
              "}\n"
              "\n"
              "void Fred::b() {\n"
              "    wilma->Reload();\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void test(int *i) {\n"
              "  if(i == NULL) { }\n"
              "  else {\n"
              "    int b = *i;\n"
              "  }\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // #2696 - false positives nr 1
        check("void f()\n"
              "{\n"
              "   struct foo *pFoo = NULL;\n"
              "   size_t len;\n"
              "\n"
              "   len = sizeof(*pFoo) - sizeof(pFoo->data);\n"
              "\n"
              "   if (pFoo)\n"
              "      bar();\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        // #2696 - false positives nr 2
        check("void f()\n"
              "{\n"
              "   struct foo *pFoo = NULL;\n"
              "   size_t len;\n"
              "\n"
              "   while (pFoo)\n"
              "      pFoo = pFoo->next;\n"
              "\n"
              "   len = sizeof(pFoo->data);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // #2696 - false positives nr 3
        check("void f()\n"
              "{\n"
              "   struct foo *pFoo = NULL;\n"
              "   size_t len;\n"
              "\n"
              "   while (pFoo)\n"
              "      pFoo = pFoo->next;\n"
              "\n"
              "   len = decltype(*pFoo);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("int foo(struct Fred *fred) {\n"
              "    if (fred) { }\n"
              "    return fred->a;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:2]: (warning) Possible null pointer dereference: fred - otherwise it is redundant to check it against null.\n", errout.str());

        // #2789 - assign and check pointer
        check("void f() {\n"
              "    char *p; p = x();\n"
              "    if (!p) { }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:3]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // check, assign and use
        check("void f() {\n"
              "    char *p;\n"
              "    if (p == 0 && (p = malloc(10)) != 0) {\n"
              "        *p = 0;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // check, assign and use
        check("void f() {\n"
              "    char *p;\n"
              "    if (p == 0 && (p = malloc(10)) != a && (*p = a)) {\n"
              "        *p = 0;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // check, and use
        check("void f() {\n"
              "    char *p;\n"
              "    if (p == 0 && (*p = 0)) {\n"
              "        return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:3]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // check, and use
        check("void f() {\n"
              "    struct foo *p;\n"
              "    if (p == 0 && p->x == 10) {\n"
              "        return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:3]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // check, and use
        check("void f() {\n"
              "    struct foo *p;\n"
              "    if (p == 0 || p->x == 10) {\n"
              "        return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // check, and use
        check("void f() {\n"
              "    char *p; p = malloc(10);\n"
              "    if (p == NULL && (*p = a)) {\n"
              "        return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:3]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        // check, and use
        check("void f(struct X *p, int x) {\n"
              "    if (!p && x==1 || p && p->x==0) {\n"
              "        return;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        {
            const char code[] = "void f(Fred *fred) {\n"
                                "    if (fred == NULL) { }\n"
                                "    fred->x();\n"
                                "}";

            check(code);    // non-inconclusive
            ASSERT_EQUALS("", errout.str());

            check(code, true);  // inconclusive
            ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:2]: (warning, inconclusive) Possible null pointer dereference: fred - otherwise it is redundant to check it against null.\n", errout.str());
        }

        check("void f(char *s) {\n"   // #3358
              "    if (s==0);\n"
              "    strcpy(a, s?b:c);\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // sizeof
        check("void f(struct fred_t *fred) {\n"
              "    if (!fred)\n"
              "        int sz = sizeof(fred->x);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // check in macro
        check("void f(int *x) {\n"
              "    $if (!x) {}\n"
              "    *x = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        // return ?:
        check("int f(ABC *p) {\n" // FP : return ?:
              "    if (!p) {}\n"
              "    return p ? p->x : 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
        check("int f(ABC *p) {\n" // no fn
              "    if (!p) {}\n"
              "    return q ? p->x : 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());
    }

    // Test CheckNullPointer::nullConstantDereference
    void nullConstantDereference() {
        // Ticket #2090
        check("void foo() {\n"
              "  strcpy(0, \"abcd\");\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        // Ticket #1171
        check("void foo(void* bar) {\n"
              "  if(strcmp(0, bar) == 0)\n"
              "    func();\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        // Ticket #2413 - it's ok to pass NULL to fflush
        check("void foo() {\n"
              "  fflush(NULL);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        // Ticket #3126 - don't confuse member function with standard function
        check("void f() {\n"
              "    image1.fseek(0, SEEK_SET);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "    int* p = 0;\n"
              "    return p[4];\n"
              "}", false, "test.cpp", false);
        ASSERT_EQUALS("[test.cpp:3]: (error) Null pointer dereference\n", errout.str());

        check("void f(int x) {\n" // #4809 - passing "NULL"
              "    itoa(x,NULL,10);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f() {\n"
              "    typeof(*NULL) y;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void f() { freopen(NULL, m, stdin); }");
        ASSERT_EQUALS("", errout.str());
    }

    void gcc_statement_expression() {
        // Ticket #2621
        check("void f(struct ABC *abc) {\n"
              "    ({ if (abc) dbg(); })\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void snprintf_with_zero_size() {
        // Ticket #2840
        check("void f() {\n"
              "    int bytes = snprintf(0, 0, \"%u\", 1);\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void snprintf_with_non_zero_size() {
        // Ticket #2840
        check("void f() {\n"
              "    int bytes = snprintf(0, 10, \"%u\", 1);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());
    }

    void printf_with_invalid_va_argument() {
        check("void f() {\n"
              "    printf(\"%s\", 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f(char* s) {\n"
              "    printf(\"%s\", s);\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "    char* s = 0;\n"
              "    printf(\"%s\", s);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Possible null pointer dereference: s\n", errout.str());

        check("void f() {\n"
              "    char *s = 0;\n"
              "    printf(\"%s\", s == 0 ? a : s);\n"
              "}", false, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "    printf(\"%u%s\", 0, 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f(char* s) {\n"
              "    printf(\"%u%s\", 0, s);\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "    char* s = 0;\n"
              "    printf(\"%u%s\", 123, s);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Possible null pointer dereference: s\n", errout.str());


        check("void f() {\n"
              "    printf(\"%%%s%%\", 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f(char* s) {\n"
              "    printf(\"text: %s, %s\", s, 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());


        check("void f() {\n"
              "    char* s = \"blabla\";\n"
              "    printf(\"%s\", s);\n"
              "}");
        ASSERT_EQUALS("", errout.str());


        check("void f(char* s) {\n"
              "    printf(\"text: %m%s, %s\", s, 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f(char* s) {\n"
              "    printf(\"text: %*s, %s\", s, 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        // Ticket #3364
        check("void f() {\n"
              "    printf(\"%-*.*s\", s, 0);\n"
              "    sprintf(\"%*\", s);\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void scanf_with_invalid_va_argument() {
        check("void f(char* s) {\n"
              "    sscanf(s, \"%s\", 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f() {\n"
              "    scanf(\"%d\", 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());

        check("void f(char* foo) {\n"
              "    char location[200];\n"
              "    int width, height;\n"
              "    sscanf(imgInfo, \"%s %d %d\", location, &width, &height);\n"
              "}");
        ASSERT_EQUALS("", errout.str()); // ticket #3207

        check("void f(char *dummy) {\n"
              "    int iVal;\n"
              "    sscanf(dummy, \"%d%c\", &iVal);\n"
              "}");
        ASSERT_EQUALS("", errout.str()); // ticket #3211

        check("void f(char *dummy) {\n"
              "    int* iVal = 0;\n"
              "    sscanf(dummy, \"%d\", iVal);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Possible null pointer dereference: iVal\n", errout.str());

        check("void f(char *dummy) {\n"
              "    int* iVal;\n"
              "    sscanf(dummy, \"%d\", foo(iVal));\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(char *dummy) {\n"
              "    int* iVal = 0;\n"
              "    sscanf(dummy, \"%d%d\", foo(iVal), iVal);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Possible null pointer dereference: iVal\n", errout.str());

        check("void f(char* dummy) {\n"
              "    sscanf(dummy, \"%*d%u\", 0);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (error) Null pointer dereference\n", errout.str());
    }

    void nullpointer_in_return() {
        check("int foo() {\n"
              "    int* iVal = 0;\n"
              "    if(g()) iVal = g();\n"
              "    return iVal[0];\n"
              "}");
        ASSERT_EQUALS("[test.cpp:4]: (error) Possible null pointer dereference: iVal\n", errout.str());

        check("int foo(int* iVal) {\n"
              "    return iVal[0];\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointer_in_typeid() {
        // Should throw std::bad_typeid
        check("struct PolymorphicA { virtual ~A() {} };\n"
              "bool foo() {\n"
              "     PolymorphicA* a = 0;\n"
              "     return typeid(*a) == typeid(*a);\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        check("struct NonPolymorphicA { ~A() {} };\n"
              "bool foo() {\n"
              "     NonPolymorphicA* a = 0;\n"
              "     return typeid(*a) == typeid(*a);\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        check("bool foo() {\n"
              "     char* c = 0;\n"
              "     return typeid(*c) == typeid(*c);\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

    }

    void nullpointer_in_for_loop() {
        // Ticket #3278
        check("void f(int* ptr, int cnt){\n"
              " if (!ptr)\n"
              "  cnt = 0;\n"
              " for (int i = 0; i < cnt; ++i)\n"
              "  *ptr++ = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointerDelete() {
        check("void f() {\n"
              "  K *k = getK();\n"
              "  if (k)\n"
              "     k->doStuff();\n"
              "  delete k;\n"
              "}\n", true);
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "  K *k = getK();\n"
              "  if (k)\n"
              "     k[0] = ptr;\n"
              "  delete [] k;\n"
              "  k = new K[10];\n"
              "}\n", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointerExit() {
        check("void f() {\n"
              "  K *k = getK();\n"
              "  if (!k)\n"
              "     exit(1);\n"
              "  k->f();\n"
              "}\n", true);
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointerStdString() {
        check("void f(std::string s1) {\n"
              "    void* p = 0;\n"
              "    s1 = 0;\n"
              "    std::string s2 = 0;\n"
              "    std::string s3(0);\n"
              "    foo(std::string(0));\n"
              "    s1 = p;\n"
              "    std::string s4 = p;\n"
              "    std::string s5(p);\n"
              "    foo(std::string(p));\n"
              "}", true);
        ASSERT_EQUALS("[test.cpp:7]: (error) Null pointer dereference\n"
                      "[test.cpp:8]: (error) Null pointer dereference\n"
                      "[test.cpp:3]: (error) Null pointer dereference\n"
                      "[test.cpp:4]: (error) Null pointer dereference\n"
                      "[test.cpp:5]: (error) Null pointer dereference\n"
                      "[test.cpp:6]: (error) Null pointer dereference\n"
                      "[test.cpp:7]: (error) Possible null pointer dereference: p\n"
                      "[test.cpp:8]: (error) Possible null pointer dereference: p\n"
                      "[test.cpp:9]: (error) Possible null pointer dereference: p\n"
                      "[test.cpp:10]: (error) Possible null pointer dereference: p\n", errout.str());

        check("void f(std::string s1, const std::string& s2, const std::string* s3) {\n"
              "    void* p = 0;\n"
              "    if (x) { return; }\n"
              "    foo(s1 == p);\n"
              "    foo(s2 == p);\n"
              "    foo(s3 == p);\n"
              "    foo(p == s1);\n"
              "    foo(p == s2);\n"
              "    foo(p == s3);\n"
              "}", true);
        ASSERT_EQUALS("[test.cpp:4]: (error) Possible null pointer dereference: p\n"
                      "[test.cpp:5]: (error) Possible null pointer dereference: p\n"
                      "[test.cpp:7]: (error) Possible null pointer dereference: p\n"
                      "[test.cpp:8]: (error) Possible null pointer dereference: p\n", errout.str());

        check("class Bar {\n"
              "    std::string s;\n"
              "    Bar() : s(0) {}\n"
              "};\n"
              "class Foo {\n"
              "    std::string s;\n"
              "    Foo();\n"
              "};\n"
              "Foo::Foo() : s(0) {}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Null pointer dereference\n"
                      "[test.cpp:9]: (error) Null pointer dereference\n", errout.str());

        check("void f() {\n"
              "    std::string s = 0 == x ? \"a\" : \"b\";\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void f() {\n"
              "  const std::string s = g();\n"
              "  ASSERT_MESSAGE(\"Error on s\", 0 == s.compare(\"Some text\"));\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }

    void nullpointerStdStream() {
        // TODO: Refactor these tests and re-enable them
        return;

        check("void f(std::ifstream& is) {\n"
              "    char* p = 0;\n"
              "    is >> p;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (error) Possible null pointer dereference: p\n", errout.str());

        check("void f(const std::ostringstream& oss, char* q) {\n"
              "    char const* p = 0;\n" // Simplification makes detection of bug difficult
              "    oss << p;\n"
              "    oss << foo << p;\n"
              "    if(q == 0)\n"
              "        oss << foo << q;\n"
              "}", false, "test.cpp", false);
        TODO_ASSERT_EQUALS("[test.cpp:3]: (error) Possible null pointer dereference: p\n"
                           "[test.cpp:4]: (error) Possible null pointer dereference: p\n"
                           "[test.cpp:6] -> [test.cpp:5]: (warning) Possible null pointer dereference: q - otherwise it is redundant to check it against null.\n",
                           "[test.cpp:6] -> [test.cpp:5]: (warning) Possible null pointer dereference: q - otherwise it is redundant to check it against null.\n", errout.str());

        check("void f(const char* p) {\n"
              "    if(p == 0) {\n"
              "        std::cout << p;\n"
              "        std::cerr << p;\n"
              "        std::cin >> p;\n"
              "        std::cout << abc << p;\n"
              "    }\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n"
                      "[test.cpp:4] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n"
                      "[test.cpp:5] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n"
                      "[test.cpp:6] -> [test.cpp:2]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

        check("void f() {\n"
              "    void* p1 = 0;\n"
              "    std::cout << p1;\n" // No char*
              "    char* p2 = 0;\n"
              "    std::cin >> (int)p;\n" // result casted
              "    std::cout << (int)p;\n"
              "}", true, "test.cpp", false);
        ASSERT_EQUALS("", errout.str());

        check("void f(const std::string& str) {\n"
              "    long long ret = 0;\n"
              "    std::istringstream istr(str);\n"
              "    istr >> std::hex >> ret;\n" // Read integer
              "    return ret;\n"
              "}", true);
        ASSERT_EQUALS("", errout.str());

        check("void f(int* i) {\n"
              "    if(i) return;\n"
              "    std::cout << i;\n" // Its no char* (#4240)
              "}", true);
        ASSERT_EQUALS("", errout.str());
    }

    void functioncall() {    // #3443 - function calls
        // dereference pointer and then check if it's null
        {
            // function not seen
            check("void f(int *p) {\n"
                  "    *p = 0;\n"
                  "    foo(p);\n"
                  "    if (p) { }\n"
                  "}");
            ASSERT_EQUALS("", errout.str());

            // function seen (taking pointer parameter)
            check("void foo(int *p) { }\n"
                  "\n"
                  "void f(int *p) {\n"
                  "    *p = 0;\n"
                  "    foo(p);\n"
                  "    if (p) { }\n"
                  "}");
            ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:6]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

            // function seen (taking reference parameter)
            check("void foo(int *&p) { }\n"
                  "\n"
                  "void f(int *p) {\n"
                  "    *p = 0;\n"
                  "    foo(p);\n"
                  "    if (p) { }\n"
                  "}", true);
            ASSERT_EQUALS("", errout.str());

            // function implementation not seen
            check("void foo(int *p);\n"
                  "\n"
                  "void f(int *p) {\n"
                  "    *p = 0;\n"
                  "    foo(p);\n"
                  "    if (p) { }\n"
                  "}");
            ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:6]: (warning) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());

            // inconclusive
            check("void f(int *p) {\n"
                  "    *p = 0;\n"
                  "    foo(p);\n"
                  "    if (p) { }\n"
                  "}", true);
            ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:4]: (warning, inconclusive) Possible null pointer dereference: p - otherwise it is redundant to check it against null.\n", errout.str());
        }

        // dereference struct pointer and then check if it's null
        {
            // function not seen
            check("void f(struct ABC *abc) {\n"
                  "    abc->a = 0;\n"
                  "    foo(abc);\n"
                  "    if (abc) { }\n"
                  "}");
            ASSERT_EQUALS("", errout.str());

            // function seen (taking pointer parameter)
            check("void foo(struct ABC *abc) { }\n"
                  "\n"
                  "void f(struct ABC *abc) {\n"
                  "    abc->a = 0;\n"
                  "    foo(abc);\n"
                  "    if (abc) { }\n"
                  "}");
            ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:6]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

            // function implementation not seen
            check("void foo(struct ABC *abc);\n"
                  "\n"
                  "void f(struct ABC *abc) {\n"
                  "    abc->a = 0;\n"
                  "    foo(abc);\n"
                  "    if (abc) { }\n"
                  "}");
            ASSERT_EQUALS("[test.cpp:4] -> [test.cpp:6]: (warning) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());

            // inconclusive
            check("void f(struct ABC *abc) {\n"
                  "    abc->a = 0;\n"
                  "    foo(abc);\n"
                  "    if (abc) { }\n"
                  "}", true);
            ASSERT_EQUALS("[test.cpp:2] -> [test.cpp:4]: (warning, inconclusive) Possible null pointer dereference: abc - otherwise it is redundant to check it against null.\n", errout.str());
        }
    }

    void functioncalllibrary() {
        Settings settings;
        Tokenizer tokenizer(&settings,this);
        std::istringstream code("void f() { int a,b; x(a,b); }");
        tokenizer.tokenize(code,"test.c");
        const Token *xtok = Token::findsimplematch(tokenizer.tokens(), "x");

        // nothing bad..
        {
            Library library;
            Library::ArgumentChecks arg;
            library.argumentChecks["x"][1] = arg;
            library.argumentChecks["x"][2] = arg;

            std::list<const Token *> null, uninit;
            CheckNullPointer::parseFunctionCall(*xtok, null, &library, 0U);
            CheckNullPointer::parseFunctionCall(*xtok, uninit, &library, 1U);
            ASSERT_EQUALS(0U, null.size());
            ASSERT_EQUALS(0U, uninit.size());
        }

        // for 1st parameter null pointer is not ok..
        {
            Library library;
            Library::ArgumentChecks arg;
            library.argumentChecks["x"][1] = arg;
            library.argumentChecks["x"][2] = arg;
            library.argumentChecks["x"][1].notnull = true;

            std::list<const Token *> null,uninit;
            CheckNullPointer::parseFunctionCall(*xtok, null, &library, 0U);
            CheckNullPointer::parseFunctionCall(*xtok, uninit, &library, 1U);
            ASSERT_EQUALS(1U, null.size());
            ASSERT_EQUALS("a", null.front()->str());
            ASSERT_EQUALS(0U, uninit.size());
        }

        // for 2nd parameter uninit data is not ok..
        {
            Library library;
            Library::ArgumentChecks arg;
            library.argumentChecks["x"][1] = arg;
            library.argumentChecks["x"][2] = arg;
            library.argumentChecks["x"][2].notuninit = true;

            std::list<const Token *> null,uninit;
            CheckNullPointer::parseFunctionCall(*xtok, null, &library, 0U);
            CheckNullPointer::parseFunctionCall(*xtok, uninit, &library, 1U);
            ASSERT_EQUALS(0U, null.size());
            ASSERT_EQUALS(1U, uninit.size());
            ASSERT_EQUALS("b", uninit.front()->str());
        }
    }

    void functioncallDefaultArguments() {

        check("void f(int *p = 0) {\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (warning) Possible null pointer dereference if the default parameter value is used: p\n", errout.str());

        check("void f(int *p = 0) {\n"
              "    if (!p)\n"
              "        return;\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(char a, int *p = 0) {\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (warning) Possible null pointer dereference if the default parameter value is used: p\n", errout.str());

        check("void f(int *p = 0) {\n"
              "    printf(\"p = %d\", *p);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (warning) Possible null pointer dereference if the default parameter value is used: p\n", errout.str());

        check("void f(int *p = 0) {\n"
              "    printf(\"p[1] = %d\", p[1]);\n"
              "}");
        ASSERT_EQUALS("[test.cpp:2]: (warning) Possible null pointer dereference if the default parameter value is used: p\n", errout.str());

        check("void f(int *p = 0) {\n"
              "    if (p != 0 && bar())\n"
              "      *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p) {\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    if (p != 0)\n"
              "      *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    int y;\n"
              "    if (p == 0)\n"
              "      p = &y;\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    if (a != 0)\n"
              "      *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (warning) Possible null pointer dereference if the default parameter value is used: p\n", errout.str());

        check("void f(int *p = 0) {\n"
              "    p = a;\n"
              "    *p = 0;\n" // <- don't simplify and verify
              "}",false,"test.cpp",false);
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    p += a;\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("int f(int *p = 0) {\n"
              "    if (p == 0) {\n"
              "        return 0;\n"
              "    }\n"
              "    return *p;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    std::cout << p ? *p : 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    std::cout << (p && p[0] ? *p : 42);\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void isEmpty(int *p = 0) {\n"
              "    return p && *p;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void g(int *p = 0) {\n"
              "    return !p || *p;\n"
              "}");
        ASSERT_EQUALS("", errout.str());


        // bar may initialize p but be can't know for sure without knowing
        // if p is passed in by reference and is modified by bar()
        check("void f(int *p = 0) {\n"
              "    bar(p);\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    printf(\"%d\", p);\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("[test.cpp:3]: (warning) Possible null pointer dereference if the default parameter value is used: p\n", errout.str());

        // The init() function may or may not initialize p, but since the address
        // of p is passed in, it's a good bet that p may be modified and
        // so we should not report an error.
        check("void f(int *p = 0) {\n"
              "    init(&p);\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void init(int* &g);\n"
              "void f(int *p = 0) {\n"
              "    init(p);\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    if (p == 0) {\n"
              "        init(&p);\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());

        check("void f(int *p = 0) {\n"
              "    if (p == 0) {\n"
              "        throw SomeException;\n"
              "    }\n"
              "    *p = 0;\n"
              "}");
        ASSERT_EQUALS("", errout.str());
    }


    void crash1() {
        check("int f() {\n"
              "    return if\n"
              "}");
    }

    void nullpointer_internal_error() { // ticket #5080
        check("struct A { unsigned int size; };\n"
              "struct B { struct A *a; };\n"
              "void f(struct B *b) {\n"
              "    unsigned int j;\n"
              "    for (j = 0; j < b[0].a->size; ++j) {\n"
              "    }\n"
              "}\n");
        ASSERT_EQUALS("", errout.str());
    }
};

REGISTER_TEST(TestNullPointer)
