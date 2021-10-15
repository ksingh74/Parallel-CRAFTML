/* MPI Program Template */

#include <stdio.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono> 
#include "mpi.h"
#include "mm.h"
using namespace std; 

#define ll long long int
#define umap unordered_map
#define pb push_back
#define ff first
#define ss second
#define sz(a) (int)a.size()
#define all(X) X.begin(),X.end()
mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

int N; // total no. of instances
int d_x; // no. of dimensions in data vector
int d_y; // no. of possible labels
const int k = 10; // branching factor

const int level_threshold = 800;
const int n_leaf = 10;

vector< unordered_map<int,double> > X; // Feature Matrix
vector< unordered_map<int,double> > Y; // Label matrix
vector< unordered_map<int,double> > Xp; // Projected feature Matrix
vector< vector<pair<int,double> > > Xp1;
vector< unordered_map<int,double> > Yp; // Projected label matrix
vector< vector<pair<int,double> > > Yp1;
vector< unordered_map<int,double> > mean; // mean vector for storing meanLabels at leaves
vector< vector<int> > child_set; // set of children ids for a given node
vector<int> pos; // reference to positions for a node id in mean vector and classifier vector
vector< vector< unordered_map<int,double> > > classifier; // stores the classifiers for each node
vector<double> Mg;
vector<bool> leaf_mrk;

int Nt;

string data_file,train_split,test_split;

vector< unordered_map<int,double> > Xt; // Feature Matrix
vector< unordered_map<int,double> > Yt; // Label matrix
vector< unordered_map<int,double> > Xpt; // Projected feature Matrix
vector< unordered_map<int,double> > Ypt; // Projected label matrix
vector<double> Mgt;
vector<double> meanvector,globalmeanvector;
vector<pair<double,int> > cmpvector;

vector<int> trainvec,testvec;

/* -------------------------------------------------------*/

int xdim,ydim,projDimx,projDimy;
ll seedIndexX,seedSignX,seedIndexY,seedSignY;
vector< vector<int> > ProjColX,ProjSignX,ProjColY,ProjSignY;

int xdimt,ydimt,projDimxt,projDimyt;
ll seedIndexXt,seedSignXt,seedIndexYt,seedSignYt;
vector<int> ProjColXt,ProjSignXt,ProjColYt,ProjSignYt;

// input processing and projection

void getInpX()
{
    ifstream in(data_file.c_str());
    string s;
    getline(in,s);
    int c = 0;
    while(c<sz(s))
    {
        if(s[c]>='0' && s[c]<='9')
        {
            N = (N*10+(s[c]-'0'));
            c++;
            continue;
        }
        c++;
        break;
    }
    while(c<sz(s))
    {
        if(s[c]>='0' && s[c]<='9')
        {
            xdim = (xdim*10+(s[c]-'0'));
            c++;
            continue;
        }
        c++;
        break;
    }
    while(c<sz(s))
    {
        if(s[c]>='0' && s[c]<='9')
        {
            ydim = (ydim*10+(s[c]-'0'));
            c++;
            continue;
        }
        c++;
        break;
    }
    int row = 0;
    while(getline(in,s))
    {
        umap<int,double> vec;
        int c = 0;
        while(1)
        {
            int v = 0;
            while(c<sz(s))
            {
                if(s[c]>='0' && s[c]<='9')
                {
                    v = (v*10+(s[c]-'0'));
                    c++;
                    continue;
                }
                break;
            }
            vec[v] = 1;
            if(s[c] == ' ')
                break;
            c++;
        }
        Y.pb(vec);
        
        vec.clear();
        c++;
        while(c<s.size())
        {
            int col = 0;
            while(s[c]!=':')
            {
                col = (col*10+s[c]-'0');
                c++;
            }
            c++;
            double d = 0;
            double val = 0;
            while(c<s.size() && s[c]!=' ')
            {
                if(s[c] == '.')
                {
                    d = 1;
                    c++;
                    continue;
                }
                val = (val*10+s[c]-'0');
                c++;
                d = d*10;
            }
            d = max(1.0,d);
            vec[col] = val/d;
            c++;
        }
        X.pb(vec);
    }
}

void intialize_seeds()
{
    ll mod = (1LL<<32)-1;
    srand(time(NULL));
    seedIndexX = (rand()*rand()*rand())%mod;
    seedIndexY = (rand()*rand()*rand())%mod;
    seedSignX = (rand()*rand()*rand())%mod;
    seedSignY = (rand()*rand()*rand())%mod;
}

ll getHash(int i,ll mod,ll seed)
{
    const int key = i;
    uint32_t p[4];
    MurmurHash3_x86_32(&key, sizeof(int), seed, &p);            
    ll res = (ll)p[0];
    return res%mod;
}

void ProjectX(int tree,int v)
{
    unordered_map<int,double> tp;
    vector<pair<int,double>> tp1;
    Xp[v] = tp;
    Xp1[v]= tp1;
    for(auto u:X[v])
        Xp[v][ProjColX[tree][u.ff]] += (double)ProjSignX[tree][u.ff]*u.ss;
    for(auto u:Xp[v])
        Xp1[v].push_back({u.ff,u.ss});
}

void ProjectY(int tree,int v)
{
    unordered_map<int,double> tp;
    vector<pair<int,double>> tp1;
    Yp[v] = tp;
    Yp1[v]= tp1;
    for(auto u:Y[v])
        Yp[v][ProjColY[tree][u.ff]] += (double)ProjSignY[tree][u.ff]*u.ss;
    for(auto u:Yp[v])
        Yp1[v].push_back({u.ff,u.ss});
}

void compute_magnitude(int i)
{
    double mg = 0;
   
    int sz=Xp1[i].size();
    // for(auto u:Xp[i])
    //     mg += u.second*u.second;
    #pragma omp parallel for num_threads(min(2,sz)) reduction(+:mg)
    for(int j=0;j<sz;j++)
        mg+=(Xp1[i][j].ss)*(Xp1[i][j].ss);
   
    Mg[i] = mg;
}

void __init(int trees)
{
    getInpX();
    
    ifstream fin (train_split);
    int f;
    while(fin >> f)
        trainvec.pb(f-1);
    
    ifstream lin (test_split);
    int l;
    while(lin >> l)
        testvec.pb(l-1);

    projDimx = min(xdim,10000);
    projDimy = min(ydim,10000);

    vector<int> tp;
    for(int j=0;j<trees;++j)
    {
        ProjColX.pb(tp);
        ProjSignX.pb(tp);
        ProjColY.pb(tp);
        ProjSignY.pb(tp);
    }

    for(int j=0;j<trees;++j)
    {
        intialize_seeds();
        for(int i = 0;i < xdim;i++)
        {
            ProjColX[j].pb(getHash(i,projDimx,seedIndexX));
            ProjSignX[j].pb(2*getHash(i,2,seedSignX)-1);
        }
        for(int i = 0;i < ydim;i++)
        {
            ProjColY[j].pb(getHash(i,projDimy,seedIndexY));
            ProjSignY[j].pb(2*getHash(i,2,seedSignY)-1);
        }
    }  

    unordered_map<int,double> utp;
    vector<pair<int,double>> utp1;
    for(int i=0;i<N;++i)
    {
        Xp.push_back(utp);
        Xp1.push_back(utp1);
        Yp.push_back(utp);
        Yp1.push_back(utp1);
    } 

    Mg.resize(N);
}

/* -------------------------------------------------------*/

int NXT_NODE_ID;

// check for leaf condition
bool test_leaf(vector<int> instances,int level)
{
    // max depth reached
    if(level == level_threshold)
    {
        // cout << "max level reached : " << level << endl;
        return 1;
    }
    
    // cardinality of the node’s instance subset is lower than a given threshold
    int n = instances.size();
    if(n <= n_leaf)
    {
        // cout << "less instances than threshold : " << n << endl;
        return 1;
    }

    
    // cout << "no. of instances : " << n << endl;
    // all the instances have the same features
    bool cont=true;
    #pragma omp parallel for num_threads(min(n,2)) reduction (&:cont)
    for(int i=1;i<n;++i)
    {
        if(!cont)
            continue;
        int cur = instances[i];
        int prv = instances[i-1];
        int d = X[cur].size();
        if(d != (int) X[prv].size())
            cont&=false;
        
        bool flag = 1;
        for(auto v:X[cur])
        {
            int dim = v.first;
            double val = v.second;
            if(abs(X[prv][dim]-val) > 1e-5)
            {
                flag = 0;
                break;
            }
        }
        if(!flag)
            cont&=false;
    }
    if(cont)
    {
        // cout << "All instances found to have same features" << endl;
        return 1;
    }

    // all the instances have the same labels
    cont=true;
    #pragma omp parallel for num_threads(min(n,2)) reduction (&:cont)
    for(int i=1;i<n;++i)
    {
        if(!cont)
            continue;
        int cur = instances[i];
        int prv = instances[i-1];
        int d = Y[cur].size();
        if(d != (int) Y[prv].size())
            cont&=false;
        
        bool flag = 1;
        for(auto v:Y[cur])
        {
            int dim = v.first;
            double val = v.second;
            if(abs(Y[prv][dim]-val) > 1e-5)
            {
                flag = 0;
                break;
            }
        }
        if(!flag)
            cont&=false;
    }
    if(cont)
    {
        // cout << "All instances found to have same labels" << endl;
        return 1;
    }


    // cout << "Check : node is non leaf." << endl;
    return 0;
}

// computes meanLabel for the leaves
unordered_map<int,double> compute_meanLabel(vector<int> instances)
{
    int n = instances.size();
    unordered_map<int,double> meanLabel;
    

    #pragma omp parallel for num_threads(min(n,2))
    for(int i=0;i<n;++i)
    {
        int cur = instances[i];
        for(auto v:Y[cur])
        {
            int dim = v.first;
            meanLabel[dim]+=(1.0/(double)(n));
        }
    }
    return meanLabel;
}

// Spherical K-means
// Instead of normal distance measure, using cosine similarity for getting closest cluster in K-means
vector<int> spherical_kmeans(vector<int> inds)
{
    // cout << "K-means ... " << (int)(inds.size())  << endl;
    // inds = Indices which have been chosen to build classifier at current node
    // k = number of clusters
    int n = inds.size();
    vector<umap<int,double>> centroids(k);
    
    // Store magnitudes of data points 
    vector<double> mags(n,0);
    #pragma omp parallel for num_threads(min(n,2))
    for(int i=0;i<n;i++)
    {
        int id = inds[i];

        double res = 0;
        int sz=Yp1[id].size();
        #pragma omp parallel for num_threads(min(2,sz)) reduction(+:res)
        for(int j=0;j<sz;j++)
            res+=(Yp1[id][j].ss)*(Yp1[id][j].ss);
        // for(auto u:Yp[id])
        //     res = res + u.ss*u.ss;
        
        mags[i] = sqrt(res);
    }

    // Choose the first k points as centroids 
    #pragma omp parallel for num_threads(min(k,2))
    for(int i=0;i<k;i++)
    {
        int id = inds[i];
        
        umap<int,double> vec;
        if(mags[i]==0)
            vec[0] = 1;
        else
        {
            for(auto u:Yp[id])
            {
                vec[u.ff] = u.ss / mags[i];                                            
            }
           
        }
        centroids[i]=vec;
    }

    vector<int> labels(n,0);
    int iterations = 0;
    while(1)
    {
        ++iterations;
        // Spherical K-means loop
        vector<umap<int,double>> new_centroids(k);
        // for(int i=0;i<k;i++)
        // {
        //     umap<int,double> vec;
        //     new_centroids.pb(vec);
        // }
        
        #pragma omp parallel for num_threads(min(n,2))
        for(int i=0;i<n;i++)                                                                                    
        {
            int id = inds[i];
            // Find the distance of point from the centroids.
            vector<pair<double,int>> dist(k);
            #pragma omp parallel for num_threads(min(k,2))
            for(int j=0;j<k;j++)
            {
                // find dot product of unit vectors of Xp[id] and centroid[j]
                double d = 0;
                for(auto u:Yp[id])                                                  //2nd Nest, so not done
                {
                    auto it = centroids[j].find(u.ff);
                    if(it!=centroids[j].end() && mags[i])
                        d = d + (u.ss*(it->ss))/mags[i];
                }
                // dist.pb({1.00-d,j});
                dist[j]={1.00-d,j};
            }
            // Assign current point to the closest cluster
            sort(all(dist));
            labels[i] = dist[0].ss;
        }

        // Find new centroids
        // #pragma omp parallel for num_threads(min(n,2))
        for(int i=0;i<n;i++)                                                                    //PROBLEM PARALLELISING
        {
            if(mags[i]!=0)
            {
                int id = inds[i];
                int cent = labels[i];
                for(auto u:Yp[id])
                {
                    new_centroids[cent][u.ff] = new_centroids[cent][u.ff] + u.ss/mags[i];
                }
            }
        }
        // Normalize the new centroids.                                                        
        #pragma omp parallel for num_threads(min(2,k))
        for(int j=0;j<k;j++)
        {
            double res = 0;
            for(auto u:new_centroids[j])
                res = res + u.ss*u.ss;
            res = sqrt(res);
            if(res==0)
            {
                res = 1;
                new_centroids[j][0] = 1;
            }
            else
            {
                
                for(auto it = new_centroids[j].begin();it!=new_centroids[j].end();it++)
                {
                it->ss /= res;
                }
            }
        }
        // Check for difference in previous and current centroid.
        bool f = true;
        double eps = 1e-8;
        #pragma omp parallel for num_threads(min(2,5)) reduction(&:f)
        for(int i=0;i<k;i++)
        {
            for(auto u:centroids[i])
            {
                double df;
                auto it = new_centroids[i].find(u.ff);
                if(it==new_centroids[i].end())
                    df = u.ss;
                else
                    df = abs(u.ss - it->ss);
                if(df > eps)
                    f&=false;
            }
            for(auto u:new_centroids[i])
            {
                double df;
                auto it = centroids[i].find(u.ff);
                if(it == centroids[i].end())
                    df = u.ss;
                else
                    df = abs(u.ss - it->ss);
                if(df > eps)
                    f&=false;
            }
        }
        if(f)
            break;
        centroids = new_centroids;
    }
    // cout << "Check : clusters built on sampled labels." << endl;
    return labels;  
}

// building classifier after performing k-means on sampled data and labels
vector<umap<int,double>> buildClassifier(vector<int> labels,vector<int> inds)
{
    // Find centroids of X
    
    vector<umap<int,double>> centroids(k);
    // for(int i=0;i<k;i++)
    // {
    //     umap<int,double> vec;
    //     centroids.pb(vec);
    // }
    int n = inds.size();
    #pragma openmp parallel for num_threads(min(2,n))
    for(int i=0;i<n;i++)
    {
        int id = inds[i];

        double mag = 0;
        int sz=Xp[id].size();
        #pragma omp parallel for num_threads(min(2,sz)) reduction(+:mag)
        for(int j=0;j<sz;j++)
            mag+=(Xp1[id][j].ss)*(Xp1[id][j].ss);
        // for(auto u:Xp[id])
        //     mag = mag + u.ss*u.ss;
        if(mag!=0)
        {
            mag = sqrt(mag);
        
            for(auto u:Xp[id])
                centroids[labels[i]][u.ff] += u.ss/mag;                                                            
        }
    }
    #pragma omp parallel for num_threads(min(2,k))
    for(int i=0;i<k;i++)
    {
        double mag = 0;
        for(auto u:centroids[i])
            mag = mag + u.ss*u.ss;
        mag = sqrt(mag);
        if(mag == 0)
            centroids[i][0] = 1;
        else
        {
            for(auto it=centroids[i].begin();it!=centroids[i].end();it++)
                it->ss /= mag;
        }
    }
    return centroids;
}

// training the node classifier
vector< unordered_map<int,double> > train_node_classifier(vector<int> inds)
{   
    shuffle(all(inds),rng); 
    int smp = min(sz(inds),20000);
    vector<int> samples(smp,0);
    #pragma omp parallel for num_threads(min(2,smp))
    for(int i=0;i<smp;i++)
    {
        // samples.pb(inds[i]);
        samples[i]=inds[i];
    }
    // Call spherical K-means to get Labels
    vector<int> labels = spherical_kmeans(samples);
    // After getting Lables call buildClassifier to get Classifier.
    vector< unordered_map<int,double> > node_classifier = buildClassifier(labels,samples); 
    // cout << "Check : Node trained, classifier built." << endl;
    return node_classifier;
}

// 1-cosine_similarity(vector a,vector b)
// flag 0 is for training and flag 1 is for testing
// p is the pos for node and v is the index of data to be classfied in train/test matrix
// i is the classifier index for the concerned node
double similarity(int u,int p,int i,int flag)
{
    double prd = 0;
    int sz=Xp1[u].size();
    // #pragma omp parallel for num_threads(min(2,sz)) reduction(+:prd)
    // for(int j=0;j<sz;j++)
    //     prd+=(Xp1[u][j].second)*(classifier[p][i][Xp1[u][j].first]);
    for(auto v:Xp[u])
        prd += v.second*classifier[p][i][v.first];
    return 1.0-(prd/Mg[u]);
}

// Given certain data points in clusters, classify function decides for test point, the most similar data point
// flag 0 is for training and flag 1 is for testing
// p is the pos for node and v is the index of data to be classfied in train/test matrix 
int classify(int p,int v,int flag)
{
    double mx = 1e12;
    int cid = 0;
    int sz=classifier[p].size();
    
    for(int i=0;i<(int)classifier[p].size();++i)                                           //PROBLEM PARALLELIZING
    {
        // to hold test point to be moved only into children containing data
        if(flag == 1 && classifier[p][i][0] == 1e9)
            continue;
        double sim = similarity(v,p,i,flag);
        if(sim < mx)
        {
            mx = sim;
            cid = i;
        }
    }
    // if(flag == 1)
    //  cout << "similarity : " << mx << endl;
    // cout << "Check : classified to " << cid << endl;
    return cid;
}

// build function for constructing the tree
int train_tree(vector<int> instances,int level)
{   
   
    int id;
    int n = instances.size();
    

    unordered_map<int,double>current_mean;
    vector<unordered_map<int,double>>current_classifier;
    vector<int> children(k,-1);
    vector< vector<int> > partition(k);
    vector<int> exist(k,0);
    bool leaf=false;
    int sum = 0;
    if(test_leaf(instances,level))
        leaf=true;
    if(!leaf)
    {
        current_classifier=train_node_classifier(instances);
        vector<int> exist(k,0);
        int flag=0;
        for(auto v:instances)
        {
            
            double mx = 1e12;
            int cid = 0;
            for(int i=0;i<(int)current_classifier.size();++i)
            {
            
                if(flag == 1 && current_classifier[i][0] == 1e9)
                    continue;
                
                double prd = 0;
                int sz=Xp1[v].size();
                // #pragma omp parallel for num_threads(min(2,sz)) reduction(+:prd)
                // for(int j=0;j<sz;j++)
                //     prd+=(Xp1[v][j].second)*(current_classifier[i][Xp1[v][j].first]);
                
                for(auto z:Xp[v])
                    prd += z.second*current_classifier[i][z.first];
                double sim=1.0-(prd/Mg[v]);
                if(sim < mx)
                {
                    mx = sim;
                    cid = i;
                }
            }
            exist[cid] = 1; 
            partition[cid].push_back(v);
        }
        #pragma omp parallel for num_threads(min(2,k)) reduction(+:sum)    
        for(int i=0;i<k;++i)
            sum += exist[i];
        
        if(sum<=1)
            current_mean=compute_meanLabel(instances);

    }
    else
        current_mean=compute_meanLabel(instances);
    
    #pragma omp critical
    {   
        id=NXT_NODE_ID;
        NXT_NODE_ID++;
        // cout<<"processing node "<<id<<"\n";
        if(leaf)
        {
            leaf_mrk.push_back(true);
            pos.push_back((int) mean.size());
            mean.push_back(current_mean);
        }
        else if(sum<=1)
        {
            leaf_mrk.push_back(true);
            pos.push_back((int) mean.size());
            mean.push_back(current_mean);
        }
        else{
            leaf_mrk.push_back(false); 
            pos.push_back((int) classifier.size());
            classifier.push_back(current_classifier);
            child_set.push_back(children);

            // cout<<child_set.size()-1 <<" "<<pos[id]<<" "<<classifier.size()-1<<" "<<id<<" "<<pos.size()-1<<"\n";

        }

    }
    if(leaf || sum<=1)
        return id;

  
    
    #pragma omp parallel for num_threads(2)
    for(int i=0;i<k;++i)
    {
        if(!(partition[i].empty()))
        {   
    
            int rid = train_tree(partition[i],level+1);
            child_set[pos[id]][i] = rid;
            
        }
        else
        {
            classifier[pos[id]][i][0] = 1e9; // INFINITE value set as an indicator
        }
    }

    // cout << "node " << id << " has " << ch << " children" << endl;
    // for(auto v:child_set[pos[id]])
    //  cout << v << " ";
    // cout << endl;
    return id;
}

double p[6];

// classify testdata
// id is the node in tree and v is the index of concerned testdata in the test_feature matrix
void test(int id,int v)
{
    // cout << "test " << id << " " << v << " " << leaf_mrk[id] << endl;
    if(leaf_mrk[id])
    {
        for(auto v:mean[pos[id]])
            meanvector[v.first] += v.second;   
        return;
    }
    int cid = classify(pos[id],v,1);
    // cout << "descends " << cid << " " << (int)(child_set[pos[id]].size()) << " " << child_set[pos[id]][cid] << endl;
    test(child_set[pos[id]][cid],v);
}

int main( int argc, char **argv ) {
    int rank, numprocs;
    NXT_NODE_ID=0;
    
    /* start up MPI */
    MPI_Init( &argc, &argv );

    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    MPI_Comm_size( MPI_COMM_WORLD, &numprocs );

    data_file = argv[1];
    train_split = argv[2];
    test_split = argv[3];
    

    /*synchronize all processes*/
    MPI_Barrier( MPI_COMM_WORLD );
    double tbeg = MPI_Wtime();

    int trees = stoi(argv[4]);
    __init(trees);

    int cols = 10;
    int Nt = 0;
    double train_time = 0;
    

    for(int split=0;split<cols;++split)
    {
        vector<int> train_instances,test_instances;
        
        for(int i=split;i<(int)(trainvec.size());i+=cols)
            train_instances.push_back(trainvec[i]);

        for(int i=split;i<(int)(testvec.size());i+=cols)
            test_instances.push_back(testvec[i]);

        Nt = test_instances.size();
        tbeg = MPI_Wtime();
        
        vector<int> roots;
        for(int i=0;i<trees;++i)
        {
            for(auto v:train_instances)
            {
                // cout << "son " << v << endl;
                ProjectX(i,v);
                ProjectY(i,v);
                compute_magnitude(v);
                // cout << "mon " << v << endl;
            }
            // cout << "training .. " << split << " " << i << endl;
            roots.push_back(train_tree(train_instances,0));
        }

        MPI_Barrier( MPI_COMM_WORLD );
        double elapsedTime = MPI_Wtime() - tbeg;
        double maxTime;
        MPI_Reduce( &elapsedTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD );
        if ( rank == 0 ) {
            printf( "total duration of training for split %d: %f\n",split, maxTime);
            train_time += maxTime;
        }

        meanvector.resize(ydim);
        globalmeanvector.resize(ydim);
        cmpvector.resize(ydim);

        MPI_Barrier( MPI_COMM_WORLD );
        

        for(int s=0;s<(int)test_instances.size();++s)
        {
            int i = test_instances[s];
            fill(meanvector.begin(),meanvector.end(),0.0);
            // cout << "classifying test " << i << endl;
            for(int j=0;j<trees;++j)
            {
                ProjectX(j,i);
                ProjectY(j,i);
                compute_magnitude(i);
                test(roots[j],i);
            }
            // cout << "classified " << i << endl;
            
            MPI_Reduce(meanvector.data(),globalmeanvector.data(),ydim,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
            if(rank == 0)
            {
                for(int j=0;j<ydim;++j)
                    cmpvector[j] = {globalmeanvector[j],j};
                sort(cmpvector.begin(),cmpvector.end());
                reverse(cmpvector.begin(),cmpvector.end());
                for(int l=1;l<=5;l+=2)
                {
                    for(int j=0;j<min(l,(int)cmpvector.size());++j)
                    {
                        if(Y[i][cmpvector[j].second])
                            p[l] += (1.0)/(double)(l);
                    }
                }
            }
        }

        mean.clear();
        child_set.clear();
        pos.clear();
        classifier.clear();
        leaf_mrk.clear();
        NXT_NODE_ID = 0;
    }

    if ( rank == 0 ) {
        printf( "total duration of training %f\n",train_time);
    }

    Nt = Nt*cols;

    if(rank == 0)
    {
        for(int i=1;i<=5;i+=2)
            cout << "p@" << i << " : " << 100.0*(p[i]/Nt) << "\n"; 
    }

    /* shut down MPI */
    MPI_Finalize();
    return 0;
}