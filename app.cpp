#include<iostream>
#include<stdio.h>
#include<string.h>

using namespace std;

typedef struct string_view{
    const char *data;
    size_t count;
}string_v;

string_v sv(const char* cstr){
    return string_v{
        .data=cstr,
        .count=strlen(cstr),
    };
}

void String_trim_right(string_v *s){
    int i = s->count -1 ;
    while(i > 0 && s->data[i] == ' '){
        i--;
    }
    s->count=i+1;
}


void String_trim_left(string_v *s){
    int i = 0;
    while(i < s->count && s->data[i] == ' '){
        i++;
    }
    for(int n = 0)
}

#define STR_V "%.*s"

#define S_ARG(s) (s).count , (s).data


int main(){
    cout<<"Hello world"<<endl;
    string_v s = sv("        hello world       ");
    printf("|");
    String_trim_right(&s);
    printf(STR_V,S_ARG(s));
    printf("|");

    return 0;
}