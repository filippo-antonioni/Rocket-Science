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


%% FILTRO KALMAN

n=length(sens.acc);
stato.x=[0;0;0]; % h v e bias
stato.P=diag([0.5^2,0.1^2, 0.4^2]); %valori lowkey campati per aria 
x_log=zeros(3,n); % h v e bias

flag=false;
flag_t=false;
deploy=false;
t_deploy=NaN; % resta NaN se la macchina a stati non apre mai il paracadute


stato_m.fase='PAD';
stato_m.x=zeros(3,1);
stato_m.count=0;
stato_m.t=0;

for i=1:n
    
    
    stato = kalman(stato,sens.acc(i),sens.baro_h(i),sens.baro_fresh(i),P);
    x_log(:,i)=stato.x; %salvo h e v all'interno del x_log (stato.x e' gia' 2x1)
    stato_m.t=sens.t(i);
    [stato_m,flag,flag_t,deploy] = macchina_stati(stato_m,stato.x(2),sens.acc(i),stato.x(1),P,deploy,flag_t);
    
    
    if flag_t
        stato_m.t_start=sens.t(i);
    end
    if flag
    
    t_deploy=sens.t(i);
    deploy=true;

    end
   
    
end

%% Apogeo: Kalman vs verita' (calcolato PRIMA dei plot, serve per i marker)
h_kal = x_log(1,:);                                            % quota stimata
v_kal = x_log(2,:);                                            % velocita' stimata
dopo_t_min = sens.t(1:end-1)' >= P.logica.t_min_deploy;       % riga, come v_kal: prima di t_min nessun apogeo e' credibile
i_apo_kal = find(v_kal(1:end-1) > 0 & v_kal(2:end) <= 0 & dopo_t_min, 1); % 1o cambio segno +->- dopo t_min
if isempty(i_apo_kal)
    fprintf('Kalman: la velocita'' non incrocia mai lo zero (controlla Q/R o i segni)\n');
    t_apo_kal = NaN; 
    h_apo_kal = NaN; 
    v_apo_kal = NaN;
    
else
    t_apo_kal = sens.t(i_apo_kal);
    h_apo_kal = h_kal(i_apo_kal);
    v_apo_kal = v_kal(i_apo_kal);
end

%% Figura QUOTA
figure;
plot(sens.t, h_kal, 'LineWidth',2, 'DisplayName','quota Kalman'); hold on; grid on;
plot(flight.t, flight.h, 'LineWidth',2, 'DisplayName','quota reale');
plot(sens.t, sens.baro_h, '.', 'DisplayName','quota baro (grezza)');
plot(flight.t_apogeo, flight.h_apogeo, 'p', 'MarkerSize',16, ...
     'MarkerFaceColor','r', 'MarkerEdgeColor','k', 'DisplayName','apogeo reale');
plot(t_apo_kal, h_apo_kal, 'o', 'MarkerSize',11, 'LineWidth',2, ...
     'MarkerFaceColor','c', 'MarkerEdgeColor','k', 'DisplayName','apogeo Kalman');
xlabel('t [s]'); ylabel('quota [m]');
title('Kalman — quota: stima vs verità');
legend('Location','bestoutside');

%% Figura VELOCITA'
figure;
plot(sens.t, v_kal, 'LineWidth',2, 'DisplayName','velocità Kalman'); hold on; grid on;
plot(flight.t, flight.v, 'LineWidth',2, 'DisplayName','velocità reale');
yline(0, 'k--', 'DisplayName','zero (apogeo)');
plot(flight.t_apogeo, 0, 'p', 'MarkerSize',16, ...
     'MarkerFaceColor','r', 'MarkerEdgeColor','k', 'DisplayName','apogeo reale');
plot(t_apo_kal, v_apo_kal, 'o', 'MarkerSize',11, 'LineWidth',2, ...
     'MarkerFaceColor','c', 'MarkerEdgeColor','k', 'DisplayName','apogeo Kalman');
xlabel('t [s]'); ylabel('velocità [m/s]');
title('Kalman — velocità: zero-crossing all''apogeo');
legend('Location','bestoutside');

%% Figura BIAS
figure;
plot(sens.t, x_log(3,:), 'LineWidth',2, 'DisplayName','bias stimato Kalman'); hold on; grid on;
yline(sens.bias_acc_vero, 'r--', 'LineWidth',2, 'DisplayName','bias vero');
xlabel('t [s]'); ylabel('bias accel [m/s^2]');
title('Kalman — convergenza del bias');
legend('Location','bestoutside');

%% ---- REPORT RISULTATI ----
err_bias = abs(sens.bias_acc_vero - stato.x(3));
scarto_t = abs(t_apo_kal - flight.t_apogeo);
scarto_h = abs(h_apo_kal - flight.h_apogeo);
if scarto_t <= 0.3, giud_apo = 'PASS'; else, giud_apo = 'FAIL'; end
if err_bias <= 0.02, giud_bias = 'PASS'; else, giud_bias = 'FAIL'; end

fprintf('\n========== REPORT KALMAN ==========\n');
fprintf('%-14s %10s %10s %10s\n', 'grandezza', 'vero', 'Kalman', 'scarto');
fprintf('%-14s %10.2f %10.2f %10.3f\n', 'apogeo t [s]', flight.t_apogeo, t_apo_kal, scarto_t);
fprintf('%-14s %10.2f %10.2f %10.3f\n', 'apogeo h [m]', flight.h_apogeo, h_apo_kal, scarto_h);
fprintf('%-14s %10.4f %10.4f %10.4f\n', 'bias [m/s^2]', sens.bias_acc_vero, stato.x(3), err_bias);
fprintf('-----------------------------------\n');
fprintf('ESITO: apogeo(<0.3s)=%s  bias(<0.02)=%s\n', giud_apo, giud_bias);
fprintf('===================================\n');
fprintf('-----------------------------------\n');
fprintf('CONFRONTO TEMPI APOGEO: \ntempo apogeo kalman:%.4f s\n tempo apogeo macchina a stati:%.4f s\n differenza tra tempi:%.4f ms \n', t_apo_kal, t_deploy,abs(t_apo_kal-t_deploy)*1e3);
fprintf('===================================\n');


