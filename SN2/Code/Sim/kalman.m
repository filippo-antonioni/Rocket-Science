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


% --- PREDIZIONE: sempre, una volta sola per campione (il tempo passa anche senza misure) ---
if isfinite(acc_i)
    stato.acc_prec = acc_i;   % ultima accelerazione valida, la riuso se arriva un NaN
else
    acc_i = stato.acc_prec;   % IMU guasta per questo campione: riuso l'ultima valida
end

u= acc_i -P.g; % trovo accelerazione perchè IMU misura forza relativa
stato.x= F*stato.x + B*u;

stato.P= F*stato.P*F' + Q;


% --- CORREZIONE: solo con baro nuovo E numerico (un NaN vale come dato non arrivato) ---
if fresh_i && isfinite(baro_i)

    z=baro_i;
    y=z - H*stato.x;
    S=H*stato.P*H' + R;

    if abs(y)>4*sqrt(S) && stato.n_scarti<P.logica.scarti

        % misura troppo lontana dalla predizione: la scarto
        stato.n_scarti=stato.n_scarti+1;

    else
        % misura buona, oppure troppi scarti di fila: la accetto per riagganciarmi
        stato.n_scarti=0;
        K=stato.P*H'/S;
        stato.x=stato.x + K*y;
        stato.P=(eye(size(stato.P))-K*H)*stato.P;
    end

end

end
