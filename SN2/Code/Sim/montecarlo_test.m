% montecarlo_test.m
% Esegue un test Monte Carlo sul simulatore

clear; close all; clc;

N_sim = 200;

success_count = 0;
failsafe_count = 0;
missed_count = 0;

results = struct('t_deploy', zeros(N_sim, 1), ...
                 'h_deploy', zeros(N_sim, 1), ...
                 't_apogeo_true', zeros(N_sim, 1), ...
                 'h_apogeo_true', zeros(N_sim, 1), ...
                 'type', strings(N_sim, 1));

fprintf('Starting Monte Carlo simulation with %d launches...\n', N_sim);

for k = 1:N_sim
    P = parameters();
    
    % Randomizza il seed per avere sempre rumore diverso in sensors_sim
    P.seed = randi([1, 100000]);
    
    % Variazioni casuali per ogni volo (uniform distribution)
    % Cd variazione +/- 10%
    P.Cd = P.Cd * (1 + (rand() - 0.5) * 0.2); 
    
    % Massa struttura variazione +/- 5%
    P.massa_struttura = P.massa_struttura * (1 + (rand() - 0.5) * 0.1);
    P.massa_secca = P.massa_struttura + P.massa_motore_vuoto;
    
    % Rumore sensori variazione +/- 20%
    P.imu.rumore_acc = P.imu.rumore_acc * (1 + (rand() - 0.5) * 0.4);
    P.baro.rumore_pa = P.baro.rumore_pa * (1 + (rand() - 0.5) * 0.4);
    
    % Eseguiamo la simulazione sopprimendo i fprintf di flight_sim
    T_out = evalc('flight = flight_sim(P);');
    sens = sensors_sim(flight, P);
    
    n = length(sens.acc);
    stato.x=[0;0;0]; 
    stato.P=diag([0.5^2,0.1^2, 0.4^2]); 
    stato.n_scarti=0;   
    stato.acc_prec=P.g; 
    
    flag=false;
    flag_t=false;
    deploy=false;
    t_deploy=NaN; 
    
    stato_m.fase='PAD';
    stato_m.x=zeros(3,1);
    stato_m.count=0;
    stato_m.count_baro=0; 
    
    deploy_type = 'MISSED';
    
    for i=1:n
        stato = kalman(stato,sens.acc(i),sens.baro_h(i),sens.baro_fresh(i),P);
        [stato_m,flag,flag_t,deploy] = macchina_stati(stato_m,stato.x(2),sens.acc(i),stato.x(1),P,deploy,flag_t,sens.t(i));
        
        if flag && isnan(t_deploy)
            t_deploy = sens.t(i);
            deploy = true;
            
            % Controllo se è un deploy di sicurezza (failsafe)
            if isfield(stato_m, 't_start') && (sens.t(i) - stato_m.t_start) >= (P.logica.t_max_deploy - 0.05)
                deploy_type = 'FAILSAFE';
                failsafe_count = failsafe_count + 1;
            else
                deploy_type = 'SUCCESS';
                success_count = success_count + 1;
            end
            break;
        end
    end
    
    if isnan(t_deploy)
        missed_count = missed_count + 1;
    end
    
    results.t_deploy(k) = t_deploy;
    if ~isnan(t_deploy)
        results.h_deploy(k) = interp1(flight.t, flight.h, t_deploy);
    else
        results.h_deploy(k) = NaN;
    end
    results.t_apogeo_true(k) = flight.t_apogeo;
    results.h_apogeo_true(k) = flight.h_apogeo;
    results.type(k) = deploy_type;
    
    if mod(k, 20) == 0
        fprintf('Completati %d / %d lanci...\n', k, N_sim);
    end
end

fprintf('\n==== RISULTATI MONTE CARLO ====\n');
fprintf('Lanci Totali: %d\n', N_sim);
fprintf('Deploy Effettivi (Apogeo): %d (%.1f%%)\n', success_count, success_count/N_sim*100);
fprintf('Deploy Failsafe (Sicurezza): %d (%.1f%%)\n', failsafe_count, failsafe_count/N_sim*100);
fprintf('Deploy Mancati: %d (%.1f%%)\n', missed_count, missed_count/N_sim*100);

idx_success = (results.type == "SUCCESS");
if sum(idx_success) > 0
    ritardo = results.t_deploy(idx_success) - results.t_apogeo_true(idx_success);
    fprintf('\nStatistiche per i deploy EFFETTIVI:\n');
    fprintf('Ritardo medio dall''apogeo vero: %.3f s\n', mean(ritardo));
    fprintf('Ritardo massimo dall''apogeo vero: %.3f s\n', max(ritardo));
    fprintf('Ritardo minimo dall''apogeo vero: %.3f s\n', min(ritardo));
end

figure;
if sum(idx_success) > 0
    histogram(results.h_apogeo_true(idx_success), 20, 'FaceColor', 'b', 'FaceAlpha', 0.5, 'DisplayName', 'Quota Apogeo Reale');
    hold on;
    histogram(results.h_deploy(idx_success), 20, 'FaceColor', 'g', 'FaceAlpha', 0.5, 'DisplayName', 'Quota Apertura Paracadute');
    legend('Location', 'best');
    title('Distribuzione Quota Apogeo vs Quota Apertura (Successi)');
    xlabel('Quota [m]');
    ylabel('Frequenza');
    grid on;
else
    disp('Nessun successo per plottare l''istogramma!');
end
