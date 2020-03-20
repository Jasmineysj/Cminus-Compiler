int call(int a[], int b[],int n){
    return b[n];
}

int main(void){
   int a[10];
   int b[5];
   b[1] = 1;
   a[0] = 10;
   b[2] = (b[1] + 5) * 2; 
   return call(a,b,2);
}