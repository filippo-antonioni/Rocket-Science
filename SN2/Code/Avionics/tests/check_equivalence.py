"""Confronto con le equazioni MATLAB, non esecuzione del simulatore originale.
Richiede Python + NumPy + g++. Eseguire dalla cartella razzo_cpp.
"""
import pathlib
import subprocess
import tempfile
import numpy as np

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    exe = str(pathlib.Path(tmp) / "replay")
    subprocess.run(["g++", "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
                    str(root / "tests/replay.cpp"), "-o", exe], check=True)
    rng = np.random.default_rng(99)
    for variable_dt in (False, True):
        x = np.zeros(3)
        P = np.diag([.25, .01, .16])
        last_acc, rejected = 9.80665, 0
        h = v = t = 0.
        inputs, expected = [], []
        phase = count = count_baro = 0
        previous_v = start_ms = 0
        deploy = False
        for i in range(6000):
            dt = float(rng.uniform(.009, .018)) if variable_dt else .01
            t += dt
            a = 0. if t < 1 else (25. if t < 3 else (-10.1 if h > 0 else 0.))
            h += v*dt + .5*a*dt*dt
            v += a*dt
            if h < 0: h = v = 0.
            acc = a + 9.80665 + .08 + rng.normal(0,.012)
            baro = h + rng.normal(0,.25)
            fresh = i % 3 == 0
            if 120 < i < 135 or i % 47 == 0: acc = float("nan")
            if 340 < i < 380: baro += 1000
            if i % 73 == 0: baro = float("nan")
            ms = round(t*1000)
            inputs.append(f"{acc} {baro} {int(fresh)} {dt} {ms}\n")
            if np.isfinite(acc): last_acc = acc
            F = np.array([[1,dt,-.5*dt*dt],[0,1,-dt],[0,0,1.]])
            B = np.array([.5*dt*dt,dt,0.])
            x = F@x + B*(last_acc-9.80665)
            P = F@P@F.T + np.outer(B,B)*.012**2 + np.diag([0,0,1e-4*dt/.01])
            correction = 0
            if fresh and np.isfinite(baro):
                y = baro-x[0]
                S = P[0,0] + (2.97*.083*10)**2
                if abs(y)>4*np.sqrt(S) and rejected<5:
                    rejected += 1
                    correction = 1
                else:
                    rejected = 0
                    K = P[:,0]/S
                    x += K*y
                    P = (np.eye(3)-np.outer(K,[1,0,0]))@P # forma MATLAB originale
                    correction = 2
            launch = opening = False
            flight_time = (ms-start_ms)*.001
            if phase == 0:
                count = count+1 if acc>30 else 0
                count_baro = count_baro+1 if x[0]>18 else 0
                if count>=5 or count_baro>=5:
                    phase=1; count=count_baro=0; start_ms=ms; launch=True
            elif phase == 1:
                count = count+1 if x[1]<previous_v and acc<0 and x[0]>1.5 else 0
                if count>=5: phase=2; count=0
                if flight_time>8 and not deploy:
                    opening=deploy=True; phase=3; count=0
            elif phase == 2:
                count = count+1 if x[1]<0 and x[0]>30 and flight_time>1.8 else 0
                if count>=5: opening=deploy=True; phase=3; count=0
                if flight_time>8 and not deploy:
                    opening=deploy=True; phase=3; count=0
            elif phase == 3:
                count=count+1 if abs(x[1])<.4 and x[0]<10 else 0
                if count>=5: phase=4; count=0
            previous_v=x[1]
            expected.append([*x,*P.flatten(),rejected,correction,phase,launch,opening,deploy])
        result = subprocess.run([exe],input="".join(inputs),text=True,capture_output=True,check=True)
        actual=np.array([[float(x) for x in line.split()] for line in result.stdout.splitlines()])
        expected=np.array(expected)
        np.testing.assert_allclose(actual[:,:12],expected[:,:12],rtol=1e-10,atol=1e-10)
        np.testing.assert_array_equal(actual[:,12:],expected[:,12:])
        error=np.max(np.abs(actual[:,:12]-expected[:,:12]))
        print(f"PASS: 6000 campioni, dt {'variabile' if variable_dt else '0.01 s'}, "
              f"errore massimo x/P={error:.3g}, fasi/eventi/scarti identici")
