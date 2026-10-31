import subprocess, random, sys, time
def gen_pos(R,C,rng,dens):
    NH=(R+1)*C; E=NH+R*(C+1)
    eb=[[-1,-1] for _ in range(E)]; be=[]
    for r in range(R):
        for c in range(C):
            b=r*C+c; es=[r*C+c,(r+1)*C+c,NH+r*(C+1)+c,NH+r*(C+1)+c+1]; be.append(es)
            for e in es:
                if eb[e][0]<0: eb[e][0]=b
                else: eb[e][1]=b
    drawn=[False]*E; sides=[0]*(R*C)
    def dr(e):
        drawn[e]=True
        for b in eb[e]:
            if b>=0: sides[b]+=1
    for b in range(R*C):
        if rng.random()<dens*0.15 and all(sides[x]<=0 for x in [b]):
            if all(not drawn[e] for e in be[b]) and all(sides[o]+sum(1 for e in be[o] if e in be[b])<=2 or o==b for o in range(R*C) if o!=b and any(e in be[o] for e in be[b])):
                for e in be[b]: dr(e)
    order=list(range(E)); rng.shuffle(order)
    for e in order:
        if drawn[e]: continue
        if rng.random()<dens and all(b<0 or sides[b]<=1 for b in eb[e]): dr(e)
    # validate no 3-sided
    if any(s==3 for s in sides) or all(drawn): return None
    rows=[]
    for r in range(R+1):
        rows.append('+'+'+'.join('-' if drawn[r*C+c] else '.' for c in range(C))+'+')
        if r<R:
            s=''
            for c in range(C+1):
                s+='|' if drawn[NH+r*(C+1)+c] else '.'
                if c<C: s+='#' if sides[r*C+c]==4 else 'o'
            rows.append(s)
    return rows,drawn,sides,eb,NH,E
def play(cmdA,cmdB,R,C,rows,drawn0,sides0,eb,NH,E,firstA):
    drawn=drawn0[:]; sides=sides0[:]; N=sum(1 for s in sides if s<4)
    P=[subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1) for cmd in (cmdA,cmdB)]
    for i,p in enumerate(P):
        f=1 if (i==0)==firstA else 0
        p.stdin.write(f"GAME {R} {C} {f}\n"+"\n".join(rows)+"\n"); p.stdin.flush()
    turn=0 if firstA else 1; score=[0,0]; tt=[0.0,0.0]; forfeit=None
    while not all(drawn):
        p=P[turn]; t0=time.time(); p.stdin.write("YOUR_TURN\n"); p.stdin.flush(); ln=p.stdout.readline().split(); dt=time.time()-t0; tt[turn]+=dt
        if dt>0.3: print("SLOW",turn,round(dt,2),R,C,sum(1 for x in drawn if not x),file=sys.stderr)
        try:
            assert ln[0]=='LINE'; r1,c1,r2,c2=map(int,ln[1:5])
            if r1>r2 or c1>c2: r1,c1,r2,c2=r2,c2,r1,c1
            if r1==r2: assert c2==c1+1 and 0<=r1<=R and 0<=c1<C; e=r1*C+c1
            else: assert r2==r1+1 and c1==c2 and 0<=r1<R and 0<=c1<=C; e=NH+r1*(C+1)+c1
            assert not drawn[e]
        except Exception as ex:
            forfeit=turn; print("FORFEIT",turn,ln,ex,file=sys.stderr); break
        drawn[e]=True; got=0
        for b in eb[e]:
            if b>=0:
                sides[b]+=1
                if sides[b]==4: got+=1
        score[turn]+=got
        P[1-turn].stdin.write(f"OPPONENT {r1} {c1} {r2} {c2}\n"); P[1-turn].stdin.flush()
        if got==0: turn=1-turn
    pts=[]
    for i in range(2):
        if forfeit is not None: v=0 if forfeit==i else 10
        else: v=max(0,min(10,5+10*(score[i]-score[1-i])/N))
        pts.append(v)
        try: P[i].stdin.write(f"GAME_END {v:.2f}\n"); P[i].stdin.close()
        except: pass
    for p in P: p.wait()
    return pts[0],score,tt
if __name__=='__main__':
    A=sys.argv[1].split(); B=sys.argv[2].split(); n=int(sys.argv[3]); seed=int(sys.argv[4]) if len(sys.argv)>4 else 1
    rng=random.Random(seed); tot=0; maxt=0; res=[]
    for g in range(n):
        while True:
            R=rng.randint(1,10); C=rng.randint(1,10)
            if rng.random()<0.5: R=rng.randint(5,10); C=rng.randint(5,10)
            pos=gen_pos(R,C,rng,rng.choice([0,0,0.2,0.4,0.7]))
            if pos: break
        rows,drawn,sides,eb,NH,E=pos
        pa,sc,tt=play(A,B,R,C,rows,drawn,sides,eb,NH,E,g%2==0)
        tot+=pa; maxt=max(maxt,tt[0]); res.append(pa)
        print(f"g{g} {R}x{C} first={g%2==0} score={sc} ptsA={pa:.2f} tA={tt[0]:.2f}",flush=True)
    print(f"AVG {tot/n:.3f}  maxtimeA {maxt:.2f}  losses {sum(1 for x in res if x<5)}")
