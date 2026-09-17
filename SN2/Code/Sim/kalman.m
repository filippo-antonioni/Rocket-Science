function stato = kalman(stato,acc_i,baro_i,fresh_i,P)

dt=1/P.imu.freq;

F=[1 dt -0.5*dt^2;
    0 1 -dt;
    0 0 1];

B=[0.5*dt^2;
    dt;
    0];

H=[1 0 0];

R=(P.baro.rumore_m*P.baro.fattore_disturbi)^2;

Q=(B*B')*P.imu.rumore_acc^2 + diag([0 0 1e-4]); %valore piccolo del bias così risulta costante al filtro e la sua incertezza converge sennò non avrei più correzione quando riaggiorno P per il bias e il filtro smetterebbe

u= acc_i -P.g; % trovo accelerazione perchè IMU misura forza relativa 
stato.x= F*stato.x + B*u;

stato.P= F*stato.P*F' + Q;


if fresh_i

    z=baro_i;
    y=z - H*stato.x;
    S=H*stato.P*H' + R;
    K=stato.P*H'/S;
    stato.x=stato.x + K*y;
    stato.P=(eye(size(stato.P))-K*H)*stato.P;

end



end