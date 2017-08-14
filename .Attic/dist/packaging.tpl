template main $END$ {{ }}
$END$

file CHANGELOG { } ../CHANGELOG

template CHANGELOG=project $END$ {{ }}
{{CHANGELOG_entry}}
$END$

template CHANGELOG_entry=changelog $END$ {{ }}
{{project}} ({{chg_version}})
{{CHANGELOG_notes}} -- {{contributor}}  {{chg_wday}}, {{chg_mday}} {{chg_month}} {{chg_year}} {{chg_hour}}:{{chg_minute}}:00 +0100

$END$

template CHANGELOG_notes=changelog $END$ {{ }}
{{CHANGELOG_note}}$END$

template CHANGELOG_note=text $END$ {{ }}
  * {{text}}
$END$

namespace Project=project
namespace project=project  locase
namespace PROJECT=project  upcase
variable contributor
variable version
variable chg_version=changelog[0]
variable chg_wday=changelog[1]
variable chg_mday=changelog[2]
variable chg_month=changelog[3]
variable chg_year=changelog[4]
variable chg_hour=changelog[5]
variable chg_minute=changelog[6]
variable text


