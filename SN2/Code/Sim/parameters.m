function P = parameters()


%% --- RAZZO ---
P.massa_struttura = 0.3234; % [kg] razzo SENZA motore, calcolata da SN2.ork (OpenRocket):
                            %      tubi+naso+pinne PLA 185 g, paracadute+cord 15 g,
                            %      elettronica 80 g, batteria 20 g, bulkhead/scudo/carica 32 g.
                            %      DA CONFERMARE CON LA BILANCIA (batteria ancora stimata)
                            % massa_secca e massa_prop vengono dal .eng, sotto
P.diametro     = 0.05;    % [m] 
P.Cd            = 0.60;    % [-] coeff. di resistenza in salita 
P.Cd_paracadute = 0.8;     % [-] Cd del paracadute aperto
P.diam_paracadute = 0.45;  % [m] diametro paracadute

P.area = pi * (P.diametro/2)^2;            % [m^2] sezione frontale
P.area_paracadute = pi * (P.diam_paracadute/2)^2;

%% --- MOTORE: Klima D9 (file ufficiale produttore da thrustcurve.org)
% Per cambiare motore: scarica il nuovo .eng, sovrascrivi dati_motore.eng, fine.
cartella = fileparts(mfilename('fullpath'));
righe = readlines(fullfile(cartella, 'dati_motore.eng'));
righe = righe(~startsWith(strtrim(righe), ';') & strlength(strtrim(righe)) > 0); 

intestazione = strsplit(strtrim(righe(1)));
P.motore_nome        = intestazione{1};                    % 'D9'
P.massa_prop         = str2double(intestazione{5});        % [kg] 0.0161 propellente
P.massa_motore_vuoto = str2double(intestazione{6}) - P.massa_prop;  % [kg] involucro che scende col paracadute
P.massa_secca        = P.massa_struttura + P.massa_motore_vuoto;    % [kg] quello che resta a burnout
                                                                    %      (flight_sim fa m = massa_secca + m_prop)

coppie = str2num(char(join(righe(2:end), ' ')));           %#ok<ST2NM>
P.curva_spinta = [0 0; reshape(coppie, 2, []).'];          % matrice Nx2 colonna1 = tempo, colonna2= spinta
P.t_burnout = P.curva_spinta(end,1);                       % [s] 

%% --- AMBIENTE ---
P.g    = 9.80665;   % [m/s^2]
P.rho0 = 1.225;     % [kg/m^3] 
P.p0   = 101325;    % [Pa]
P.quota_rampa = 0;  % [m] 

%% --- SIMULAZIONE ---
P.dt_fisica = 0.001;   % [s] passo di integrazione della "verita'" (1 kHz)
P.t_max     = 60;      % [s] durata massima simulazione a caso

%% --- SENSORI: barometro BMP280 ---
% Fonte: datasheet Bosch BST-BMP280-DS001 rev 1.26 + impostazioni in main.cpp
% (MODE_NORMAL, temp x1, pressione x8 = "High resolution", FILTER_X4,
% standby 0.5 ms). Ogni valore cita la tabella da cui viene.

P.baro.freq = 38;   %trovato claude facendo girare da banco          % [Hz] Tabella 13 (pag.19): con osrs_p x8 una misura
                              %      dura 19.5-22.5 ms -> Tabella 14: ODR ~44-50 Hz.
                              %      Arrotondo in BASSO (40): nel dubbio simulo un
                              %      sensore un po' peggiore del vero, mai migliore.
                              %      E' la frequenza della CORREZIONE del Kalman.
                              % confermata frequenza da test a banco

P.baro.rumore_datasheet_pa = 0.6;  % [Pa] Tabella 8 (pag.15): RMS noise con
                                   %      "High resolution" (x8) + IIR coeff 4.
                                   %      Misurato pero' da Bosch "in a controlled
                                   %      pressure environment" (par. 3.5): in LAB.


P.baro.fattore_disturbi    = 10;   % [-]  In volo non sei in lab: vento sui fori
                                   %      di sfiato, vibrazioni, transitori termici.
                                   %      x10 e' un margine onesto; potrai misurarlo
                                   %      davvero dai log con analizza_log_reale.m



P.baro.rumore_pa = 2.97; % [Pa] misurato dal banco 

P.baro.rumore_m= 2.97 * 0.083; % [m] prodotto dalla misurazione da banco 


P.baro.bias_pa   = 12;        % [Pa] pag.8: "Relative accuracy" Arel = +/-0.12 hPa
                              %      (l'accuratezza ASSOLUTA e' +/-1 hPa = 100 Pa,
                              %      ma a te non interessa: usi la quota RELATIVA,
                              %      e la calibrazione a terra toglie l'offset.
                              %      Resta l'errore relativo, che simulo qui.)

P.baro.ritardo_s = 0.09;      % [s] par. 3.3.3: il filtro IIR fa
                              %      y = y_old*(c-1)/c + x/c  con c = coefficiente.
                              %      Con FILTER_X4: costante di tempo ~ c/ODR =
                              %      4/44 Hz ~ 0.09 s: il baro ti dice dove il razzo
                              %      era ~90 ms fa. Con X16 sarebbe ~0.36 s: ecco
                              %      perche' in main.cpp siamo scesi da X16 a X4.
P.baro.c=4;
%% --- SENSORI: IMU ICM-42670-P ---
% Fonte: datasheet TDK DS-000451 + impostazioni in main.cpp
% (startAccel(100, 16), startGyro(100, 2000)).
P.imu.freq          = 100;    % [Hz] scelto io, confermato da test a banco 
P.imu.fondoscala_g  = 16;     % [g] da startAccel(..., 16): oltre 157 m/s^2 il
                              %     sensore SATURA e tu perdi il dato vero

P.imu.rumore_acc    = 0.012;  % preso da misurazioni sul banco



P.imu.bias_acc      = 0.25;   % [m/s^2] Tabella 2, sezione ZERO-G OUTPUT:
                              %     "Initial Tolerance +/-25 mg" a 25 gradi
                              %     ~ 0.25 m/s^2, diverso a ogni accensione.
                              %     E' il motivo per cui il Kalman ha il TERZO
                              %     stato: stima il bias e lo toglie da se'
                           
                              
                              %NOTA questo sarebbe da cambiare facendo un pò
                            %di prove da banco e calcolando i vari bias
                            %trovati e facendo una media, attualmente è
                            %0.0819 dal primo log a banco ma potrebbe
                            %essere di più


P.imu.vibrazioni    = 1.5;    % [m/s^2] 
                              % qui per toglierlo serve una misurazione a
                              % motore acceso che avremo solo al primo
                              % lancio 
%% Giroscopio misurato a banco

P.gyr.freq = 100;        % [Hz]  ODR del giroscopio (come startGyro(100, ...))


P.gyr.bias.x = -0.6954;  % [dps] (gradi/secondo) bias di accensione asse x, misurato a banco
P.gyr.bias.y = 0.1532;   % [dps] bias asse y
P.gyr.bias.z = -0.6130;  % [dps] bias asse z

P.gyr.rumore = 0.04;     % [dps] deviazione standard del rumore (misurata, ~0.038-0.043 sui 3 assi)

%% --- LOGICA DI VOLO (soglie della macchina a stati) ---
P.logica.soglia_decollo   = 30;    % [m/s^2] accel misurata per dichiarare LIFTOFF (~3g)
P.logica.n_conferme       = 5;     % [-] campioni consecutivi per confermare un evento
P.logica.quota_min_deploy = 30;    % [m] 
P.logica.t_min_deploy     = 1.8;   % [s]
P.logica.t_max_deploy     = 8;    % [s] failsafe

%% MERDA PER I RANDOM IN SIMULA SENS

P.seed=99; %completamente a caso
end
