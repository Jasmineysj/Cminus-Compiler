void output(int a){
    return ;
}
int call(int a){
    output(a);
}
int main(void){
    int a[10];
    int b;
    a[1] = 1;
    a[ a[1] = 1 ] = 1;
    call (b = 1);
    return 1;
}