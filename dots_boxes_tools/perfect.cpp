#include <bits/stdc++.h>
using namespace std; typedef unsigned long long u64;
int R,C,NH,E,NBX; int eb[220][2], be[100][4];
unordered_map<u64,int> memo;
int boxesDone(u64 m,int e){int c=0; for(int j=0;j<2;j++){int b=eb[e][j]; if(b<0)continue; bool ok=true; for(int k=0;k<4;k++) if(!(m>>be[b][k]&1)) ok=false; if(ok)c++;} return c;}
int solve(u64 m){ if(m==(E==64?~0ULL:((1ULL<<E)-1))) return 0; auto it=memo.find(m); if(it!=memo.end()) return it->second;
  int best=INT_MIN; for(int e=0;e<E;e++) if(!(m>>e&1)){ u64 n=m|1ULL<<e; int c=boxesDone(n,e); int v= c? c+solve(n) : -solve(n); best=max(best,v);} memo[m]=best; return best;}
int main(){ char buf[256]; fgets(buf,256,stdin); int first; sscanf(buf,"GAME %d %d %d",&R,&C,&first);
 NH=(R+1)*C;E=NH+R*(C+1);NBX=R*C; for(int e=0;e<E;e++)eb[e][0]=eb[e][1]=-1;
 for(int r=0;r<R;r++)for(int c=0;c<C;c++){int b=r*C+c;int es[4]={r*C+c,(r+1)*C+c,NH+r*(C+1)+c,NH+r*(C+1)+c+1};for(int k=0;k<4;k++){be[b][k]=es[k];if(eb[es[k]][0]<0)eb[es[k]][0]=b;else eb[es[k]][1]=b;}}
 u64 m=0; vector<string> rows(2*R+1); for(auto&x:rows){fgets(buf,256,stdin);x=buf;}
 for(int r=0;r<=R;r++)for(int c=0;c<C;c++)if(rows[2*r][2*c+1]=='-')m|=1ULL<<(r*C+c);
 for(int r=0;r<R;r++)for(int c=0;c<=C;c++)if(rows[2*r+1][2*c]=='|')m|=1ULL<<(NH+r*(C+1)+c);
 mt19937 rng(7);
 while(fgets(buf,256,stdin)){string s=buf;
  if(s.rfind("YOUR_TURN",0)==0){ int best=INT_MIN; vector<int> bm; for(int e=0;e<E;e++) if(!(m>>e&1)){u64 n=m|1ULL<<e; int c=boxesDone(n,e); int v=c?c+solve(n):-solve(n); if(v>best){best=v;bm.clear();} if(v==best)bm.push_back(e);}
    int mv=bm[rng()%bm.size()]; m|=1ULL<<mv; int r1,c1,r2,c2; if(mv<NH){r1=r2=mv/C;c1=mv%C;c2=c1+1;}else{int x=mv-NH;r1=x/(C+1);c1=c2=x%(C+1);r2=r1+1;}
    printf("LINE %d %d %d %d\n",r1,c1,r2,c2); fflush(stdout);}
  else if(s.rfind("OPPONENT",0)==0){int a,b,c,d;sscanf(buf,"OPPONENT %d %d %d %d",&a,&b,&c,&d); int e=(a==c)?a*C+min(b,d):NH+min(a,c)*(C+1)+b; m|=1ULL<<e;}
  else break;}}
