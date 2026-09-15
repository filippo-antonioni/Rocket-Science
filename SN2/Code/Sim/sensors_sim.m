function sens = sensors_sim(flight,P)

rng(P.seed); 

dt = 1/ P.imu.freq;
t= [0:dt:flight.t(end)]';
n=length(t);

h_real=interp1(flight.t,flight.h,t);
v_real=interp1(flight.t,flight.v,t);
acc_real=interp1(flight.t,flight.acc,t);
p_real=P.p0 * (1 - 2.25577e-5 * (h_real + P.quota_rampa)).^5.25588;  % ISA

sens.h_real=h_real;
sens.v_real=v_real;
sens.acc_real=acc_real;
sens.p_real=p_real;
sens.P=P;

%% IMU

acc_ideal= acc_real + P.g;
bias= P.imu.bias_acc *(2*rand -1); % 2*rand -1 serve solo per riportare il range del bias simmetrico centrato in 0 cosi il numero di rand è tra -1 e 1
rumore = P.imu.rumore_acc * randn(n,1); % uso randn perchè rumore su singola misura ha distribuzione gaussiana quindi pesco da li, speri docet
index_array=find(t<=flight.t_burnout);
n2=index_array(end);
vibr= P.imu.vibrazioni * randn(n2,1);


acc_mis = acc_ideal + bias + rumore;
acc_mis(1:n2) = acc_mis(1:n2) +vibr;

fs = P.imu.fondoscala_g * P.g;
acc_mis = max(min(acc_mis,fs),-fs); % check che non abbia saturato anche se con il motore attuale è impossibile

sens.t=t;
sens.acc=acc_mis;
sens.bias_acc_vero = bias;  % verita' iniettata: la stima Kalman del bias deve convergere qui

%% GIROSCOPIO
% Sim 1D = nessuna rotazione -> verita' = 0. L'uscita e' puro errore.
gyro_true = 0; % non misuro nulla nel mio flight_sim anche perchè è 1D

gyr_x = gyro_true + P.gyr.bias.x + P.gyr.rumore*randn(n,1);
gyr_y = gyro_true + P.gyr.bias.y + P.gyr.rumore*randn(n,1);
gyr_z = gyro_true + P.gyr.bias.z + P.gyr.rumore*randn(n,1);


sens.gyro = [gyr_x, gyr_y, gyr_z];



%% BAROMETRO

h_baro=zeros(n,1);
fresh=false(n,1);
dt_baro=1/P.baro.freq;
t_prox=0;

tau = P.baro.ritardo_s;
p_filt=zeros(n,1);
p_filt(1)=p_real(1);

for i=2:n

    p_filt(i)= p_filt(i-1)*tau/(tau+dt) + p_real(i)*dt/(tau+dt);

end

for i=1:n

    %check se ho dato nuovo dal barometro
    if t(i)>=t_prox
        
        p_mis=p_filt(i) + P.baro.rumore_pa*randn + P.baro.bias_pa;
        h_baro(i)=44330*(1 - (p_mis/P.p0)^0.1903); %formula ISA
        fresh(i)=true;
        t_prox=t_prox + dt_baro;
    
    
    else 
    
        h_baro(i)=h_baro(i-1);
        
    
    end

end



sens.baro_h=h_baro;
sens.baro_fresh=fresh;



end