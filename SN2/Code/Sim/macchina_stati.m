function [stato_m,flag,flag_t,deploy] = macchina_stati(stato_m,v_kalman_i,forza_specifica_i,h_kalman_i,P,deploy,flag_t)


flag_t=false;
flag=false;


switch stato_m.fase 
    case 'PAD'
                   
         if forza_specifica_i>P.logica.soglia_decollo
            stato_m.count=stato_m.count+1; 

            if stato_m.count==P.logica.n_conferme
                
                
                stato_m.fase='BOOST';
                stato_m.count=0;
                flag_t=true;
            
            end
         else 
             stato_m.count=0;
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
          if (stato_m.t-stato_m.t_start)> P.logica.t_max_deploy && ~deploy
            
                flag=true;
                stato_m.fase='DESCENT';
                deploy=true;
            
         end
            
    case 'COAST'
        
            if v_kalman_i<0 && h_kalman_i>P.logica.quota_min_deploy && (stato_m.t-stato_m.t_start)>P.logica.t_min_deploy
                
                stato_m.count=stato_m.count+1;
                
                if stato_m.count==P.logica.n_conferme
                    flag=true;
                    stato_m.fase='DESCENT';
                    deploy=true;
                
                end
            else 
                stato_m.count=0;
                
            end
           
            
            if (stato_m.t-stato_m.t_start)> P.logica.t_max_deploy && ~deploy
            
                flag=true;
                stato_m.fase='DESCENT';
                deploy=true;
            
            end
    case 'DESCENT'
        
            
            
end


stato_m.x=[h_kalman_i;v_kalman_i;forza_specifica_i];

end