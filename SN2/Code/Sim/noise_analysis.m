% Script per calcolare rumori, frequenza reale, errori dei sensori da prova
% sul banco 


clear
close all
clc

%% Caricamento dati 
cartella = fileparts(mfilename('fullpath'));
percorso = fullfile(cartella, '..', 'Avionics', 'logs', 'device-monitor-260813-153005.log'); % log incluso nella repository

righe = readlines(percorso);
dati= [];

for i = 1: numel(righe)

    n = sscanf(righe(i),"%f,%f,%f,%f,%f,%f,%f,%f"); %leggo riga per riga 8 float che sono i dati presi dai sensori e non tutto il resto 
    if length(n) == 8
        
        dati(end+1,:) = n';
    end
    


end

%% Conversione dati

t = (dati(:,1)-dati(1,1))/1000;           %[s] log è in ms
P = dati(:,2);                            % [Pa]
acc = dati(:,3:5)/ 2048 * 9.80665;        % [m/s^2]
gyr = dati (:, 6:8)/16.4;                 % [dps]
alt = 44330*(1-(P/101325).^0.190295);     % [m] conversione con ISA
N_campioni_zero = 200; % scelgo un numero di campioni per calcolare quota 0
alt = alt - mean(alt(1:N_campioni_zero)); % zero = media iniziale, uso solo i primi 200 campioni per capire il riferimento da terra, su BANCO sono tutti uguali ma in volo no

%% Pulizia dati corrotti (dropout / glitch)

%a riposo il modulo dell'accelerazione vale ~9.81
% a riposo il giroscopio non gira
accel_mag = vecnorm(acc, 2, 2);                       % modulo accel per campione
valido = (abs(accel_mag - 9.80665) < 1) & all(abs(gyr) < 5, 2);
n_scartati = sum(~valido);

t   = t(valido);      alt = alt(valido);      P = P(valido);
acc = acc(valido, :); gyr = gyr(valido, :);

%% Calcolo frequenza e rumore dei sensori


diff_t = diff(t);
freq = 1 / median(diff_t); %[Hz]



noise_alt= std(alt); % Deviazione standard quota per il calcolo del rumore 
noise_acc = std(acc); % Deviazione standard accelerazioni su tre assi 
noise_gyr = std(gyr); % Deviazione standrd giroscopio

% Bias accel: a riposo il MODULO del vettore medio deve valere 1g (9.81).
% Uso il modulo e non solo l'asse Z perche' la scheda non e' perfettamente in bolla:
% un'inclinazione sposta un po' di g da Z verso X/Y e falserebbe la stima.
acc_media = mean(acc);                              % [ax ay az] medi
bias_acc  = norm(acc_media) - 9.80665;              % scostamento del modulo da 1g
inclinazione_deg = acosd(acc_media(3)/norm(acc_media)); % quanto la scheda e' fuori bolla
bias_gyr= mean (gyr); % Bias del giroscopio sui 3 assi


%% Grafici 
% grazie mille Claude 
set(groot, 'defaultTextInterpreter', 'latex');
set(groot, 'defaultAxesTickLabelInterpreter', 'latex');
set(groot, 'defaultLegendInterpreter', 'latex');
set(groot, 'defaultAxesFontSize', 12);
set(groot, 'defaultLineLineWidth', 1);

cX = [0.00 0.45 0.74];   % blu
cY = [0.85 0.33 0.10];   % arancio
cZ = [0.47 0.67 0.19];   % verde
cB = [0.49 0.18 0.56];   % viola
lettere = {'x','y','z'}; colori = {cX, cY, cZ};

% --- Figura 1: accelerometro nel tempo (un asse per riga, per vedere il rumore) ---
figure('Name','Accelerometro','Position',[80 60 860 640]);
for k = 1:3
    subplot(3,1,k); hold on; grid on; box on;
    plot(t, acc(:,k), 'Color', colori{k});
    yline(mean(acc(:,k)), '--', 'Color', [.4 .4 .4]);
    ylabel(['$a_' lettere{k} '$ [m/s$^2$]']);
    title(['Asse $' lettere{k} '$~~($\sigma = ' sprintf('%.4f', std(acc(:,k))) '$ m/s$^2$)']);
    if k == 3, xlabel('Tempo $t$ [s]'); end
end
sgtitle('\textbf{Accelerometro ICM-42670-P a riposo}', 'Interpreter', 'latex');

% --- Figura 2: giroscopio nel tempo ---
figure('Name','Giroscopio','Position',[120 60 860 640]);
for k = 1:3
    subplot(3,1,k); hold on; grid on; box on;
    plot(t, gyr(:,k), 'Color', colori{k});
    yline(mean(gyr(:,k)), '--', 'Color', [.4 .4 .4]);
    ylabel(['$\omega_' lettere{k} '$ [dps]']);
    title(['Asse $' lettere{k} '$~~($\sigma = ' sprintf('%.4f', std(gyr(:,k))) '$ dps,~ bias $= ' sprintf('%.3f', mean(gyr(:,k))) '$ dps)']);
    if k == 3, xlabel('Tempo $t$ [s]'); end
end
sgtitle('\textbf{Giroscopio ICM-42670-P a riposo}', 'Interpreter', 'latex');

% --- Figura 3: quota barometrica nel tempo ---
figure('Name','Barometro','Position',[160 100 860 400]);
hold on; grid on; box on;
plot(t, alt, 'Color', cB);
yline(0, '--', 'Color', [.4 .4 .4]);
xlabel('Tempo $t$ [s]'); ylabel('Quota relativa [m]');
title(['\textbf{Barometro BMP280 a riposo}~~($\sigma = ' sprintf('%.3f', std(alt)) '$ m)']);

% --- Figura 4: istogramma del rumore dell'accel. Z + gaussiana teorica ---
figure('Name','Istogramma rumore','Position',[200 100 720 460]);
hold on; grid on; box on;
r = acc(:,3) - mean(acc(:,3));                 % rumore centrato sulla media
histogram(r, 60, 'Normalization', 'pdf', 'FaceColor', cZ, 'EdgeColor', 'none', 'FaceAlpha', 0.7);
xg = linspace(min(r), max(r), 200);  s = std(r);
plot(xg, exp(-xg.^2/(2*s^2))/(s*sqrt(2*pi)), 'Color', cX, 'LineWidth', 2);
xlabel('Scarto dalla media [m/s$^2$]'); ylabel('PDF');
title('\textbf{Distribuzione del rumore -- accel. asse $z$}');
legend({'dati misurati', 'gaussiana teorica'}, 'Location', 'northeast');


%% Stampa valori
fprintf('\n========== ANALISI RUMORE SENSORI (test da fermo) ==========\n');
fprintf('Campioni totali:       %d\n', size(dati,1));
fprintf('Campioni scartati:     %d (dropout/glitch)\n', n_scartati);
fprintf('Campioni usati:        %d\n', numel(t));
fprintf('Durata acquisizione:   %.1f s\n', t(end));
fprintf('Frequenza reale:       %.2f Hz\n', freq);
fprintf('------------------------------------------------------------\n');
fprintf('BAROMETRO\n');
fprintf('  rumore quota (std):  %.3f m   (= %.2f Pa)\n', noise_alt, std(P));
fprintf('------------------------------------------------------------\n');
fprintf('ACCELEROMETRO [m/s^2]         X          Y          Z\n');
fprintf('  rumore (std):         %9.4f  %9.4f  %9.4f\n', noise_acc(1), noise_acc(2), noise_acc(3));
fprintf('  bias (|a| - g):       %.4f m/s^2\n', bias_acc);
fprintf('  inclinazione scheda:  %.2f gradi\n', inclinazione_deg);
fprintf('------------------------------------------------------------\n');
fprintf('GIROSCOPIO [dps]             X          Y          Z\n');
fprintf('  rumore (std):         %9.4f  %9.4f  %9.4f\n', noise_gyr(1), noise_gyr(2), noise_gyr(3));
fprintf('  bias (mean):          %9.4f  %9.4f  %9.4f\n', bias_gyr(1), bias_gyr(2), bias_gyr(3));
fprintf('============================================================\n');







