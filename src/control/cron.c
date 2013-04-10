//
//  cron.c
//  AnalyserDude
//
//  Created by Peter Shaw on 3/31/13.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifndef IGNOREINTESTS
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

#include "cron.h"

char nextidentifyer;

/** 
  * Increment the clock
  * -------------------------
  * called from ISR(TIMER0_COMPA_vect)
  */
void cron_increment_clock(void){
    ++system_millisecunds_since_startup;
    ++cron_ms_for_execution_timer;
    if(system_millisecunds_since_startup == 86400000){
        // 24h
        system_millisecunds_since_startup = 0;
        ++system_days_since_startup;
        if(system_days_since_startup == 255){
            system_days_since_startup = 0;
        }
    }
}

/**
 * Should a job start
 * -------------------------
 * called from ISR(TIMER0_COMPA_vect)
 */
void cron_execute_jobs(void){
    // check first
    if(cron_ms_for_execution_timer < crontab_ptr->first->total_ms){
        // too low, nothing can ever match
        return;
    }
    // reset on end
    if(cron_ms_for_execution_timer > crontab_ptr->last->total_ms){
        cron_ms_for_execution_timer = 1;
    }
    // check elements
    cron_joblist_t  *current;
    current         = crontab_ptr->first;
    while(current != NULL &&  current->total_ms <= cron_ms_for_execution_timer){
        if( (cron_ms_for_execution_timer % current->total_ms) == 0 ){
            current->fn();
        }
        current = current->next;
    }
}

/**
  * Interupt Servce Routine
  * -------------------------
  * This ISR will called every millisecond. 
  * It compared the time with the crontab
  * and call the functions if nessasary.
  */
#ifndef IGNOREINTESTS
ISR (TIMER0_COMPA_vect) {
#else
void interuptServiceRoutine(void){
#endif
    // tick the clock
    cron_increment_clock();
    // should execute a job?
    cron_execute_jobs();
}

void cron_calculate_uptime_hms(){
    // ms +(sec *1000) +(min *60 *1000) +(hours *60 *60 *1000);
    uint32_t ms;
    ms = system_millisecunds_since_startup;
    cron_hours      = floor( ms /60 /60 /1000);
    cron_minutes    = floor( (ms -(cron_hours *60 *60 *1000)) /60 /1000);
    cron_seconds    = floor( ((ms -(cron_hours *60 *60 *1000))-(cron_minutes *60 *1000)) /1000);
}

void cron_init(void){
    // clear global timer
    system_millisecunds_since_startup   = 0;
    system_days_since_startup           = 0;
    cron_seconds                        = 0;
    cron_minutes                        = 0;
    cron_hours                          = 0;
    
    // set the identifyer, will be increased by every new job
    nextidentifyer = 1;
    
    // initialize the crontab
    crontab_ptr = malloc(sizeof(cron_borders_t));
    crontab_ptr->first                  = NULL;
    crontab_ptr->last                   = NULL;
    
#ifndef IGNOREINTESTS
    // timer (timer0) setup
    TCCR0A = (1<<WGM01);                    // CTC Modus
    TCCR0B |= (1<<CS01) | (1<<CS00);        // Prescaler 64
    
    /** if the cpu does not run on the default 16MHZ, than calculate
      * a timer comparator for 1ms
      */
#if F_CPU!=16000000UL
#error ************************************************************
#error Other CPU types, less than 16MHZ are not supportet right now
#error ------------------------------------------------------------
#error Please visit github.com/petershaw/AnalyzerDude
#error for updates or patches. 
#error ************************************************************    
#else
    // if the cpu works with 16MHZ than use the default comparator.
    // ((16000000/64)/1000) = 250
    OCR0A = 250-1;
#endif
    
    //  start Compare Interrupt
    /** atmega328p for example needs TIMSK0 */
#if defined(TIMSK0)
    TIMSK0 |= (1<<OCIE0A);
#else
    TIMSK |= (1<<OCIE0A);
#endif
#endif //IGNOREINTESTS
}

char cron_add_job(int ms, int sec, int min, int hours, void *fn){
    // create a new job
    cron_joblist_t *newCronJob  = malloc(sizeof(cron_joblist_t));
    newCronJob->identifyer      = nextidentifyer++;
    newCronJob->fn              = fn;

    // sort by time
    uint32_t total_ms           = ms +(sec *1000) +(min *60 *1000) +(hours *60 *60 *1000);
    
    newCronJob->total_ms        = total_ms;
    newCronJob->next            = NULL;

    // move first/last and last next
    if (crontab_ptr->last       == NULL) {
        // first element
        crontab_ptr->last       = newCronJob;
        crontab_ptr->first      = newCronJob;
    } else if(crontab_ptr->last->total_ms < total_ms) {
        // add to the end
        crontab_ptr->last->next = newCronJob;
        crontab_ptr->last       = newCronJob;
    } else {
        // sort the element in the right order
        cron_joblist_t  *current;
        cron_joblist_t  *previous = NULL;
        current         = crontab_ptr->first;
        while (current != NULL){
            if(total_ms <= current->total_ms){
                if(previous == NULL){
                    crontab_ptr->first = newCronJob;
                    crontab_ptr->first->next = current;
                    break;
                } else {
                    previous->next = newCronJob;
                    newCronJob->next = current;
                    break;
                }
            }
            previous = current;
            current = current->next;
        }
    }
    
    
    
    return newCronJob->identifyer;
}

void cron_remove_job(char jobid){
    cron_joblist_t  *current;
    cron_joblist_t  *previous;
    cron_joblist_t  *next;
    previous        = NULL;
    next            = NULL;
    current         = crontab_ptr->first;
    while (current  != NULL) {
        if(current->identifyer == jobid){
            if(current == crontab_ptr->first){
                crontab_ptr->first = current->next;
            } else {
                next = current->next;
                previous->next = next;
            }
            free(current);
            break; 
        } else {
            previous    = current;
            current     = current->next;
        }
    }
}

void cron_clear(void){
    cron_joblist_t *current;
    current = crontab_ptr->first;
    while (crontab_ptr->first != NULL) {
        current = crontab_ptr->first;
        crontab_ptr->first = current->next;
        free(current);
    }
    crontab_ptr->last = NULL;
}

int cron_count(void) {
    int cnt;
    cron_joblist_t *current;
    cnt = 0;
    current = crontab_ptr->first;
    while (current != NULL) {
        ++cnt;
        current = current->next;
    }
    return cnt;
}
