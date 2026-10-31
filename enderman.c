#include <stdio.h>
#include <stdlib.h>

static char buf[1<<25];
static int bp = 0, bl = 0;
static int gc(void){ if(bp==bl){ bl=fread(buf,1,sizeof buf,stdin); bp=0; if(bl<=0) return -1;} return buf[bp++]; }
static int rd(void){ int c=gc(); while(c!='-'&&(c<'0'||c>'9')){ if(c==-1) return 0; c=gc(); }
    int s=1; if(c=='-'){s=-1;c=gc();} int x=0; while(c>='0'&&c<='9'){x=x*10+c-'0';c=gc();} return x*s; }

int n, m;
int *head, *nxt, *to;
int *tin, *tout, *low, *par, *comp, *it, *stk, *ord;
int *cstart, *ch;

static int piece(int x, int e){
    if(!(tin[x] > tin[e] && tin[x] <= tout[e])) return 0;
    int lo = cstart[e], hi = cstart[e+1]-1;
    while(lo < hi){ int mid=(lo+hi+1)/2; if(tin[ch[mid]] <= tin[x]) lo=mid; else hi=mid-1; }
    int c = ch[lo];
    return (low[c] >= tin[e]) ? c : 0;
}

int main(void){
    n = rd(); m = rd();
    head = malloc((n+2)*sizeof(int)); for(int i=0;i<=n+1;i++) head[i]=-1;
    nxt = malloc((2*m+2)*sizeof(int)); to = malloc((2*m+2)*sizeof(int));
    int ec=0;
    for(int i=0;i<m;i++){ int a=rd(), b=rd();
        to[ec]=b; nxt[ec]=head[a]; head[a]=ec++;
        to[ec]=a; nxt[ec]=head[b]; head[b]=ec++; }
    tin=calloc(n+2,sizeof(int)); tout=calloc(n+2,sizeof(int)); low=calloc(n+2,sizeof(int));
    par=calloc(n+2,sizeof(int)); comp=calloc(n+2,sizeof(int)); it=malloc((n+2)*sizeof(int));
    stk=malloc((n+2)*sizeof(int)); ord=malloc((n+2)*sizeof(int));
    int timer=0, cc=0;
    for(int v=1; v<=n; v++) it[v]=head[v];
    for(int r=1; r<=n; r++){
        if(tin[r]) continue;
        cc++; int sp=0; stk[sp++]=r; par[r]=0; tin[r]=low[r]=++timer; ord[timer]=r; comp[r]=cc;
        while(sp){
            int v=stk[sp-1];
            if(it[v]!=-1){
                int e=it[v]; it[v]=nxt[e]; int u=to[e];
                if(!tin[u]){ par[u]=v; comp[u]=cc; tin[u]=low[u]=++timer; ord[timer]=u; stk[sp++]=u; }
                else if(tin[u]<low[v]) low[v]=tin[u];
            } else {
                tout[v]=timer; sp--;
                if(sp){ int p=stk[sp-1]; if(low[v]<low[p]) low[p]=low[v]; }
            }
        }
    }

    cstart=calloc(n+3,sizeof(int)); ch=malloc((n+2)*sizeof(int));
    for(int v=1; v<=n; v++) if(par[v]) cstart[par[v]+1]++;
    for(int v=1; v<=n+1; v++) cstart[v]+=cstart[v-1];
    int *fill=malloc((n+2)*sizeof(int));
    for(int v=0; v<=n; v++) fill[v]=cstart[v];
    for(int t=1; t<=n; t++){ int v=ord[t]; if(par[v]) ch[fill[par[v]]++]=v; }

    int q=rd();
    char *out=malloc((size_t)q*4+16); int op=0;
    while(q--){
        int c=rd(), d=rd(), e=rd(); int ok;
        if(c==e||d==e) ok=0;
        else if(c==d) ok=1;
        else if(comp[c]!=comp[d]) ok=0;
        else if(comp[e]!=comp[c]) ok=1;
        else ok = (piece(c,e)==piece(d,e));
        if(ok){ out[op++]='S';out[op++]='i';out[op++]='m'; }
        else  { out[op++]='N';out[op++]='a';out[op++]='o'; }
        out[op++]='\n';
    }
    fwrite(out,1,op,stdout);
    return 0;
}
