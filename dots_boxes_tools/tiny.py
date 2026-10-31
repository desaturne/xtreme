import judge,random,sys
rng=random.Random(int(sys.argv[1])); n=0; bad=0; tot=0
for g in range(int(sys.argv[2])):
    while True:
        R=rng.randint(1,3); C=rng.randint(1,4)
        if rng.random()<0.3: R,C=rng.choice([(1,10),(10,1),(10,10),(2,10)])
        pos=judge.gen_pos(R,C,rng,rng.choice([0,0.5,0.9,1.0]))
        if pos: break
    rows,drawn,sides,eb,NH,E=pos
    pa,sc,tt=judge.play(["./cfinal"],["./opp",str(rng.randint(0,1))],R,C,rows,drawn,sides,eb,NH,E,g%2==0)
    tot+=pa; n+=1
    if pa<5: bad+=1
print("avg",tot/n,"below5",bad)
