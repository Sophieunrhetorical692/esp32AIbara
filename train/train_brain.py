#!/usr/bin/env python3
"""
Capybara Brain v3 - Deep NN with behavior history
26 inputs -> 96 hidden -> 48 hidden -> 18 outputs
Trained on 500K samples with reinforcement-style rewards
"""
import numpy as np, random, os

BEHAVIORS = [
    "idle","walk_L","walk_R","eat_grass","swim","sleep","yawn",
    "scratch","jump","mikan","sniff","stretch","butterfly",
    "melon","look","shy","doze","run"
]
N_BEH = len(BEHAVIORS)

# 26 inputs: 21 base + 5 history (one-hot encoded as fraction of each behavior in last 5)
N_INPUTS = 26
N_H1 = 96
N_H2 = 48
N_OUT = N_BEH

def expert_policy(state):
    hp,en,hg,cur,lon = state[0],state[1],state[2],state[3],state[4]
    is_night = state[7]
    is_lake,is_forest,is_home = state[8],state[9],state[10]
    presence = state[11]
    prev_sleep,prev_eat,prev_active = state[12],state[13],state[14]
    temp,rain = state[15],state[16]
    hg_trend,en_trend,hp_trend = state[17],state[18],state[19]
    time_in_beh = state[20]
    # History features (21-25): how much of each category in recent history
    hist_sleep = state[21]  # fraction of sleep/doze in last 5
    hist_eat = state[22]    # fraction of eat in last 5
    hist_active = state[23] # fraction of active in last 5
    hist_explore = state[24] # fraction of sniff/look/butterfly
    hist_idle = state[25]    # fraction of idle/scratch/mikan

    scores = np.ones(N_BEH) * 0.3

    # === CRITICAL NEEDS ===
    if hg > 0.85: scores[3]+=9; scores[13]+=7
    elif hg > 0.65: scores[3]+=hg*5.5; scores[13]+=hg*4.5; scores[10]+=2 if not prev_eat else 0
    elif hg > 0.45: scores[3]+=hg*2.5; scores[13]+=hg*2
    if hg < 0.3: scores[3]*=0.05; scores[13]*=0.05

    if en < 0.15: scores[5]+=12; scores[16]+=6
    elif en < 0.35:
        scores[5]+=(1-en)*7; scores[16]+=(1-en)*4; scores[6]+=(1-en)*3
        if not prev_sleep: scores[6]+=3; scores[11]+=2
    elif en < 0.5: scores[6]+=2; scores[16]+=1.5
    if en > 0.6: scores[5]*=0.02; scores[16]*=0.05

    # === EMOTIONAL ===
    boredom = 1.0-hp
    fun = boredom * en
    if hp < 0.25: scores[8]+=6; scores[4]+=fun*6; scores[17]+=fun*5; scores[12]+=fun*5
    elif hp < 0.5: scores[8]+=fun*3.5; scores[4]+=fun*4; scores[12]+=fun*3.5

    if cur > 0.7: scores[10]+=cur*7; scores[14]+=cur*5; scores[12]+=cur*3
    elif cur > 0.45: scores[10]+=cur*3.5; scores[14]+=cur*2.5
    if cur < 0.25: scores[10]*=0.1; scores[14]*=0.15

    if lon > 0.6: scores[15]+=lon*3.5
    if presence: scores[8]+=2.5; scores[15]+=2; scores[9]+=2

    # === CHAINS ===
    if prev_sleep:
        scores[11]+=5; scores[6]+=3; scores[14]+=2.5
        scores[5]*=0.08; scores[16]*=0.15
    if prev_eat:
        scores[3]*=0.08; scores[13]*=0.08
        scores[0]+=3.5; scores[1]+=2; scores[2]+=2; scores[9]+=2
    if prev_active:
        scores[16]+=3.5; scores[0]+=2.5; scores[6]+=2
        scores[17]*=0.15; scores[8]*=0.2; scores[4]*=0.3

    # === HISTORY-BASED VARIETY ===
    # If too much of one category recently, reduce it
    if hist_sleep > 0.4: scores[5]*=0.3; scores[16]*=0.4  # Too much sleeping
    if hist_eat > 0.3: scores[3]*=0.3; scores[13]*=0.3     # Too much eating
    if hist_active > 0.5: scores[17]*=0.3; scores[8]*=0.3; scores[4]*=0.4
    if hist_explore > 0.4: scores[10]*=0.4; scores[14]*=0.4
    if hist_idle > 0.5: scores[0]*=0.3; scores[7]*=0.4     # Too idle, do something!
    # Boost underrepresented categories
    if hist_active < 0.1 and en > 0.5: scores[17]+=1.5; scores[8]+=1.5
    if hist_explore < 0.1: scores[10]+=1.5; scores[14]+=1

    # === ENVIRONMENT ===
    if is_lake: scores[4]+=5
    else: scores[4]*=0.08
    if is_home: scores[5]+=2.5; scores[9]+=3; scores[4]*=0.01; scores[17]*=0.3
    if is_forest: scores[10]+=3.5; scores[14]+=2.5; scores[12]+=2.5

    # === WEATHER ===
    if temp > 0.5: scores[4]+=5; scores[17]*=0.4
    elif temp < -0.3: scores[5]+=2.5; scores[9]+=2.5; scores[4]*=0.01
    if rain: scores[5]+=2.5; scores[16]+=2; scores[14]+=1.5

    if is_night: scores[5]+=6; scores[16]+=3.5; scores[17]*=0.1; scores[8]*=0.15

    # Trends
    if hg_trend > 0.5: scores[3]+=2.5; scores[13]+=2
    if en_trend < -0.5: scores[5]+=2; scores[6]+=1.5

    # Walking/comfort always somewhat valid
    scores[1]+=1.3; scores[2]+=1.3; scores[7]+=0.8+boredom*0.5
    scores[9]+=(1-hg)*1.3; scores[0]+=0.6

    if en < 0.12: scores[17]*=0.003; scores[8]*=0.008; scores[4]*=0.01

    noise = np.random.exponential(0.2, N_BEH)
    scores = np.clip(scores + noise, 0.001, None)
    return scores / scores.sum()

def gen_data(n=500000):
    X,Y = [],[]
    for _ in range(n):
        s = np.zeros(N_INPUTS)
        w = random.random()
        s[0]=np.clip(w+random.gauss(0,0.25),0,1)
        s[1]=np.clip(random.random(),0,1)
        s[2]=np.clip(random.random(),0,1)
        s[3]=np.clip(random.random(),0,1)
        s[4]=np.clip(1-w+random.gauss(0,0.2),0,1)
        h=random.randint(0,23)
        s[5]=np.sin(h/24*6.283); s[6]=np.cos(h/24*6.283); s[7]=float(h>=22 or h<6)
        e=random.randint(0,3); s[8]=float(e==1); s[9]=float(e==2); s[10]=float(e==3)
        s[11]=float(random.random()>0.4)
        s[12]=float(random.random()>0.7); s[13]=float(random.random()>0.8); s[14]=float(random.random()>0.75)
        s[15]=random.gauss(0,0.5); s[16]=float(random.random()>0.7)
        s[17]=random.gauss(0.3,0.3); s[18]=random.gauss(-0.2,0.3); s[19]=random.gauss(0,0.3)
        s[20]=random.random()
        # Generate history features
        hist = [random.randint(0,17) for _ in range(5)]
        sleep_c = sum(1 for h in hist if h in [5,16])/5
        eat_c = sum(1 for h in hist if h in [3,13])/5
        act_c = sum(1 for h in hist if h in [4,8,17])/5
        expl_c = sum(1 for h in hist if h in [10,12,14])/5
        idle_c = sum(1 for h in hist if h in [0,7,9])/5
        s[21]=sleep_c; s[22]=eat_c; s[23]=act_c; s[24]=expl_c; s[25]=idle_c

        p = expert_policy(s)
        a = np.random.choice(N_BEH, p=p)
        t = p*0.2; t[a]+=0.8; t/=t.sum()
        X.append(s); Y.append(t)
    return np.array(X), np.array(Y)

def train(X, Y, epochs=3000, lr=0.05, bs=4096):
    W1=np.random.randn(N_INPUTS,N_H1)*np.sqrt(2/N_INPUTS)
    b1=np.zeros(N_H1)
    W2=np.random.randn(N_H1,N_H2)*np.sqrt(2/N_H1)
    b2=np.zeros(N_H2)
    W3=np.random.randn(N_H2,N_OUT)*np.sqrt(2/N_H2)
    b3=np.zeros(N_OUT)
    best_l,best_w = 999,None
    n=X.shape[0]
    for ep in range(epochs):
        idx=np.random.choice(n,bs,replace=False)
        Xb,Yb=X[idx],Y[idx]
        h1=np.maximum(0,Xb@W1+b1)
        h2=np.maximum(0,h1@W2+b2)
        lo=h2@W3+b3; e=np.exp(lo-lo.max(1,keepdims=True)); out=e/e.sum(1,keepdims=True)
        loss=-np.mean(np.sum(Yb*np.log(out+1e-8),1))
        if loss<best_l: best_l=loss; best_w=(W1.copy(),b1.copy(),W2.copy(),b2.copy(),W3.copy(),b3.copy())
        do=(out-Yb)/bs
        dW3=h2.T@do; db3=do.sum(0); dh2=do@W3.T; dh2[h2<=0]=0
        dW2=h1.T@dh2; db2=dh2.sum(0); dh1=dh2@W2.T; dh1[h1<=0]=0
        dW1=Xb.T@dh1; db1=dh1.sum(0)
        cl=lr*(1-ep/epochs)+0.001
        W1-=cl*dW1; b1-=cl*db1; W2-=cl*dW2; b2-=cl*db2; W3-=cl*dW3; b3-=cl*db3
        if ep%200==0: print(f"  Ep {ep:4d} Loss:{loss:.4f}")
    print(f"  Best: {best_l:.4f}")
    return best_w

def export(w, fn):
    W1,b1,W2,b2,W3,b3 = w
    with open(fn,'w') as f:
        f.write("#pragma once\n// Capybara Brain v3\n")
        f.write(f"// {W1.shape[0]}->{W1.shape[1]}->{W2.shape[1]}->{W3.shape[1]}\n")
        f.write(f"// 500K samples, 3000 epochs\n\n")
        f.write(f"#define BRAIN_N_IN {W1.shape[0]}\n#define BRAIN_N_HIDDEN1 {W1.shape[1]}\n")
        f.write(f"#define BRAIN_N_HIDDEN2 {W2.shape[1]}\n#define BRAIN_N_OUT {W3.shape[1]}\n\n")
        def wa(nm,a):
            fl=a.flatten()
            f.write(f"const float {nm}[] PROGMEM = {{")
            f.write(",".join(f"{v:.5f}f" for v in fl))
            f.write("};\n\n")
        wa("brain_w1",W1); wa("brain_b1",b1); wa("brain_w2",W2)
        wa("brain_b2",b2); wa("brain_w3",W3); wa("brain_b3",b3)
        t=W1.size+b1.size+W2.size+b2.size+W3.size+b3.size
        f.write(f"// {t} params, {t*4} bytes\n")
    print(f"  {t} params ({t*4} bytes)")

def test(w):
    W1,b1,W2,b2,W3,b3 = w
    def pred(s):
        s=np.array(s)
        h1=np.maximum(0,s@W1+b1); h2=np.maximum(0,h1@W2+b2)
        o=h2@W3+b3; e=np.exp(o-o.max()); return e/e.sum()
    #                                hp  en  hg  cur lon sin cos ngt lk  fr  hm  pr  ps  pe  pa  tmp rn  hgt ent hpt tib hs  he  ha  hx  hi
    tests=[
        ("Starving",                [.5, .6, .95,.3, .2, 0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  .8, -.1,0,  .5, 0,  0,  0,  .2, .2]),
        ("Exhausted night",         [.4, .08,.3, .2, .3, 0,  -1, 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  -.5,0,  .5, .2, 0,  0,  0,  .2]),
        ("Bored+energy lake",       [.15,.9, .2, .8, .5, 1,  0,  0,  1,  0,  0,  0,  0,  0,  0,  .5, 0,  0,  0,  -.3,.5, 0,  0,  .2, .2, .2]),
        ("After sleep",             [.5, .8, .5, .4, .3, .5, .5, 0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  .2, -.1,0,  0,  .6, 0,  0,  0,  .2]),
        ("Forest curious",          [.6, .6, .3, .9, .2, 1,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  .5, 0,  0,  0,  .2, .2]),
        ("Cold rain home",          [.5, .4, .5, .3, .4, 0,  -1, 1,  0,  0,  1,  0,  0,  0,  0,  -.7,1,  .3, -.3,-.1,.5, .2, 0,  0,  0,  .4]),
        ("Owner+bored",             [.3, .7, .4, .6, .8, 1,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  -.2,.5, 0,  0,  .2, .2, .4]),
        ("After lots of sleeping",  [.5, .9, .4, .5, .3, .5, .5, 0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  .5, .8, 0,  0,  0,  .2]),
    ]
    print("\n  === Tests ===")
    for nm,s in tests:
        p=pred(s); top=np.argsort(p)[-4:][::-1]
        print(f"  {nm}:")
        for i in top: print(f"    {BEHAVIORS[i]:12s} {p[i]*100:5.1f}%")
        print()

if __name__=="__main__":
    print("=== Brain v3 (26->96->48->18) ===\n")
    print("1. Generating 500K...")
    X,Y = gen_data(500000)
    print("2. Training...")
    w = train(X,Y,epochs=3000,lr=0.05)
    print("\n3. Testing...")
    test(w)
    print("4. Exporting...")
    export(w, os.path.join(os.path.dirname(__file__),'..','src','brain_weights.h'))
    print("\nDone!")
