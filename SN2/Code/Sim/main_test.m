clear;
close all;
clc;


P=parameters();
flight=flight_sim(P);
sens = sensors_sim(flight,P);


% Controllo barometro
figure;
plot(flight.t,flight.h,'g','LineWidth',2);
hold on;
plot(sens.t,sens.baro_h,'.');
legend('quota vera','quota simulata baro','Location','best');
grid on;
xlabel('t [s]');
ylabel('quota [m]');
title('Controllo barometro');   


% Controllo IMU

figure;
plot(flight.t,flight.acc + P.g,'g','LineWidth',2);
hold on;
plot(sens.t,sens.acc,'-','LineWidth',0.5);   %
legend('forza specifica ideale (acc+g)','acc simulata IMU','Location','best');
grid on;
xlabel('t [s]');
ylabel('accelerazione [m/s^2]');
title('Controllo IMU');


%Controllo giroscopio
% Sim 1D: rotazione vera = 0, quindi vedi solo bias (offset) + rumore.
figure;
plot(sens.t,sens.gyro(:,1),'-'); hold on;
plot(sens.t,sens.gyro(:,2),'-');
plot(sens.t,sens.gyro(:,3),'-');
yline(0,'g--');
legend('gyro x','gyro y','gyro z','zero vero','Location','best');
grid on;
xlabel('t [s]');
ylabel('velocita'' angolare [dps]');
title('Controllo giroscopio (solo bias + rumore)');
