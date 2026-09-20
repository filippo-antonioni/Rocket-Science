function [stato_m,flag,flag_t,deploy] = macchina_stati(stato_m,v_kalman_i,forza_specifica_i,h_kalman_i,P,deploy,flag_t,t)


flag_t=false;
flag=false;


switch stato_m.fase 
    case 'PAD'
                   
         if forza_specifica_i>P.logica.soglia_decollo
            stato_m.count=stato_m.count+1; 

            
         else 
             stato_m.count=0;
         end

         % decollo di riserva dal barometro, con un contatore SUO (se l'IMU e' guasta
         % la condizione su f e' sempre falsa e azzererebbe un contatore condiviso)
         if h_kalman_i>18 % valore arbitrario che sicuro il barometro non legge da fermo sulla rampa
             stato_m.count_baro=stato_m.count_baro+1;
         else
             stato_m.count_baro=0;
         end

         if stato_m.count==P.logica.n_conferme || stato_m.count_baro==P.logica.n_conferme

             stato_m.fase='BOOST';
             stato_m.count=0;
             stato_m.count_baro=0;
             flag_t=true;
             stato_m.t_start=t;

         end
          

    case 'BOOST'

         if v_kalman_i<stato_m.x(2) && forza_specifica_i<0 && h_kalman_i>P.lunghezza_rampa
            stato_m.count=stato_m.count+1;
            if stato_m.count==P.logica.n_conferme
                stato_m.fase='COAST';
                stato_m.count=0;

            end

         else
         stato_m.count=0;

         end
         % failsafe SENZA conferma: il tempo non e' rumoroso, se e' vero lo resta
          if (t-stato_m.t_start)> P.logica.t_max_deploy && ~deploy

                flag=true;
                stato_m.fase='DESCENT';
                stato_m.count=0;
                deploy=true;

         end

    case 'COAST'

            if v_kalman_i<0 && h_kalman_i>P.logica.quota_min_deploy && (t-stato_m.t_start)>P.logica.t_min_deploy

                stato_m.count=stato_m.count+1;

                if stato_m.count==P.logica.n_conferme
                    flag=true;
                    stato_m.fase='DESCENT';
                    stato_m.count=0;
                    deploy=true;

                end
            else
                stato_m.count=0;

            end

            % failsafe SENZA conferma: il tempo non e' rumoroso, se e' vero lo resta
            if (t-stato_m.t_start)> P.logica.t_max_deploy && ~deploy

                flag=true;
                stato_m.fase='DESCENT';
                stato_m.count=0;
                deploy=true;

            end
    case 'DESCENT'

        if abs(v_kalman_i)<0.4 && h_kalman_i<10 
        stato_m.count=stato_m.count+1;
        
        if stato_m.count==P.logica.n_conferme
            stato_m.fase='LANDED';
            stato_m.count=0;
        end
            
        else 
        
        stato_m.count=0;
        end
            
            
end


stato_m.x=[h_kalman_i;v_kalman_i;forza_specifica_i];

end