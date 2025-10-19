/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at:
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*/

#ifndef FETOS_APPS_H_
#define FETOS_APPS_H_



#pragma once

// Clock app
void clock_app_run(void);
void clock_app_bg(void);
void clock_app_fg(void);
void clock_app_cleanup(void);

// Heart app
void fetos_heart_app_run(void);
void fetos_heart_app_bg(void);
void fetos_heart_app_fg(void);
void fetos_heart_app_cleanup(void);

// Status app
void status_app_run(void);
void status_app_bg(void);
void status_app_fg(void);
void status_app_cleanup(void);


#endif /* FETOS_APPS_H_ */