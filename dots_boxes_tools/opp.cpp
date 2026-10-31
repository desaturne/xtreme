// baseline opponents: mode 0 random, 1 greedy (take everything, random safe, smallest sacrifice)
#include <bits/stdc++.h>
using namespace std;
int R,C,NH,E,NBX; int eb[220][2], be[100][4]; bool drawn[220]; int sides[100];
int other(int e,int b){return eb[e][0]==b?eb[e][1]:eb[e][0];}
void dr(int e){drawn[e]=1;for(int j=0;j<2;j++)if(eb[e][j]>=0)sides[eb[e][j]]++;}
void ud(int e){drawn[e]=0;for(int j=0;j<2;j++)if(eb[e][j]>=0)sides[eb[e][j]]--;}
mt19937 rng;
int giveaway(int e){ // boxes opponent gets by greedy chain-follow after we draw e
  vector<int> st; dr(e); st.push_back(e); int cnt=0; bool any=true;
  while(any){any=false; for(int b=0;b<NBX;b++) if(sides[b]==3){ for(int k=0;k<4;k++) if(!drawn[be[b][k]]){int x=be[b][k]; dr(x); st.push_back(x); cnt++; any=true; break;} } }
  for(int i=st.size()-1;i>=0;i--) ud(st[i]); return cnt;}
int main(int argc,char**argv){int mode=atoi(argv[1]); char buf[256]; string s;
 fgets(buf,256,stdin); int first; sscanf(buf,"GAME %d %d %d",&R,&C,&first); rng.seed(R*1000+C+first*7+atoi(argc>2?argv[2]:"0"));
 NH=(R+1)*C;E=NH+R*(C+1);NBX=R*C; for(int e=0;e<E;e++)eb[e][0]=eb[e][1]=-1;
 for(int r=0;r<R;r++)for(int c=0;c<C;c++){int b=r*C+c;int es[4]={r*C+c,(r+1)*C+c,NH+r*(C+1)+c,NH+r*(C+1)+c+1};for(int k=0;k<4;k++){be[b][k]=es[k];if(eb[es[k]][0]<0)eb[es[k]][0]=b;else eb[es[k]][1]=b;}}
 vector<string> rows(2*R+1); for(auto&x:rows){fgets(buf,256,stdin);x=buf;}
 for(int r=0;r<=R;r++)for(int c=0;c<C;c++)if(rows[2*r][2*c+1]=='-')dr(r*C+c);
 for(int r=0;r<R;r++)for(int c=0;c<=C;c++)if(rows[2*r+1][2*c]=='|')dr(NH+r*(C+1)+c);
 while(fgets(buf,256,stdin)){s=buf;
  if(s.rfind("YOUR_TURN",0)==0){ vector<int> und; for(int e=0;e<E;e++)if(!drawn[e])und.push_back(e); int mv=und[rng()%und.size()];
    if(mode>=1){ int cap=-1; for(int b=0;b<NBX;b++)if(sides[b]==3)for(int k=0;k<4;k++)if(!drawn[be[b][k]])cap=be[b][k];
      if(cap>=0)mv=cap; else { vector<int> safe; for(int e:und){bool ok=true;for(int j=0;j<2;j++)if(eb[e][j]>=0&&sides[eb[e][j]]>=2)ok=false; if(ok)safe.push_back(e);} 
        if(!safe.empty())mv=safe[rng()%safe.size()]; else {int bg=1e9; for(int e:und){int g=giveaway(e); if(g<bg)bg=g,mv=e;}} } }
    dr(mv); int r1,c1,r2,c2; if(mv<NH){r1=r2=mv/C;c1=mv%C;c2=c1+1;}else{int x=mv-NH;r1=x/(C+1);c1=c2=x%(C+1);r2=r1+1;}
    printf("LINE %d %d %d %d\n",r1,c1,r2,c2); fflush(stdout);}
  else if(s.rfind("OPPONENT",0)==0){int a,b,c,d;sscanf(buf,"OPPONENT %d %d %d %d",&a,&b,&c,&d); int e=(a==c)?a*C+min(b,d):NH+min(a,c)*(C+1)+b; dr(e);}
  else break;}
}
