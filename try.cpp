#include<bits/stdc++.h>
#include<omp.h>
using namespace std;


int main(){
    int arr[]={4,1,-6,23,3,9};

    int n=6;
    int mx=INT_MIN;
    #pragma omp parallel for num_threads(6) reduction(max:mx)
    for(int i=0;i<n;i++)
    {
        
        if(arr[i]>mx)
            mx=arr[i];
        #pragma omp critical
        cout<<i<<" "<<omp_get_thread_num()<<" "<<mx<<"\n";
    }
    cout<<mx<<"\n";
}