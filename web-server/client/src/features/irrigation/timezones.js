// Timezone picker data for the Irrigation schedule.
//
// Source is a Windows-style zone list (each row has a Windows `value`, fixed
// `offset`, `isdst`, a display `text`, and a `utc[]` array of the real IANA
// zones). The scheduler + server work in IANA (Intl, DST-safe), so we resolve
// each row to ONE representative IANA and store THAT — the Windows value/offset
// are display-only. Derivation (done once at module load below):
//   1. drop rows with an empty utc[] (unresolvable),
//   2. resolve each row to an IANA via a city-match heuristic,
//   3. dedupe by resolved IANA (collapses DST-duplicate rows like PST/PDT).

const RAW_ZONES = [
  { value: 'Dateline Standard Time', offset: -12, text: '(UTC-12:00) International Date Line West', utc: ['Etc/GMT+12'] },
  { value: 'UTC-11', offset: -11, text: '(UTC-11:00) Coordinated Universal Time-11', utc: ['Etc/GMT+11', 'Pacific/Midway', 'Pacific/Niue', 'Pacific/Pago_Pago'] },
  { value: 'Hawaiian Standard Time', offset: -10, text: '(UTC-10:00) Hawaii', utc: ['Etc/GMT+10', 'Pacific/Honolulu', 'Pacific/Johnston', 'Pacific/Rarotonga', 'Pacific/Tahiti'] },
  { value: 'Alaskan Standard Time', offset: -8, text: '(UTC-09:00) Alaska', utc: ['America/Anchorage', 'America/Juneau', 'America/Nome', 'America/Sitka', 'America/Yakutat'] },
  { value: 'Pacific Standard Time (Mexico)', offset: -7, text: '(UTC-08:00) Baja California', utc: ['America/Santa_Isabel'] },
  { value: 'Pacific Daylight Time', offset: -7, text: '(UTC-07:00) Pacific Daylight Time (US & Canada)', utc: ['America/Los_Angeles', 'America/Tijuana', 'America/Vancouver'] },
  { value: 'Pacific Standard Time', offset: -8, text: '(UTC-08:00) Pacific Standard Time (US & Canada)', utc: ['America/Los_Angeles', 'America/Tijuana', 'America/Vancouver', 'PST8PDT'] },
  { value: 'US Mountain Standard Time', offset: -7, text: '(UTC-07:00) Arizona', utc: ['America/Creston', 'America/Dawson', 'America/Dawson_Creek', 'America/Hermosillo', 'America/Phoenix', 'America/Whitehorse', 'Etc/GMT+7'] },
  { value: 'Mountain Standard Time (Mexico)', offset: -6, text: '(UTC-07:00) Chihuahua, La Paz, Mazatlan', utc: ['America/Chihuahua', 'America/Mazatlan'] },
  { value: 'Mountain Standard Time', offset: -6, text: '(UTC-07:00) Mountain Time (US & Canada)', utc: ['America/Boise', 'America/Cambridge_Bay', 'America/Denver', 'America/Edmonton', 'America/Inuvik', 'America/Ojinaga', 'America/Yellowknife', 'MST7MDT'] },
  { value: 'Central America Standard Time', offset: -6, text: '(UTC-06:00) Central America', utc: ['America/Belize', 'America/Costa_Rica', 'America/El_Salvador', 'America/Guatemala', 'America/Managua', 'America/Tegucigalpa', 'Etc/GMT+6', 'Pacific/Galapagos'] },
  { value: 'Central Standard Time', offset: -5, text: '(UTC-06:00) Central Time (US & Canada)', utc: ['America/Chicago', 'America/Indiana/Knox', 'America/Indiana/Tell_City', 'America/Matamoros', 'America/Menominee', 'America/North_Dakota/Beulah', 'America/North_Dakota/Center', 'America/North_Dakota/New_Salem', 'America/Rainy_River', 'America/Rankin_Inlet', 'America/Resolute', 'America/Winnipeg', 'CST6CDT'] },
  { value: 'Central Standard Time (Mexico)', offset: -5, text: '(UTC-06:00) Guadalajara, Mexico City, Monterrey', utc: ['America/Bahia_Banderas', 'America/Cancun', 'America/Merida', 'America/Mexico_City', 'America/Monterrey'] },
  { value: 'Canada Central Standard Time', offset: -6, text: '(UTC-06:00) Saskatchewan', utc: ['America/Regina', 'America/Swift_Current'] },
  { value: 'SA Pacific Standard Time', offset: -5, text: '(UTC-05:00) Bogota, Lima, Quito', utc: ['America/Bogota', 'America/Cayman', 'America/Coral_Harbour', 'America/Eirunepe', 'America/Guayaquil', 'America/Jamaica', 'America/Lima', 'America/Panama', 'America/Rio_Branco', 'Etc/GMT+5'] },
  { value: 'Eastern Standard Time', offset: -5, text: '(UTC-05:00) Eastern Time (US & Canada)', utc: ['America/Detroit', 'America/Havana', 'America/Indiana/Petersburg', 'America/Indiana/Vincennes', 'America/Indiana/Winamac', 'America/Iqaluit', 'America/Kentucky/Monticello', 'America/Louisville', 'America/Montreal', 'America/Nassau', 'America/New_York', 'America/Nipigon', 'America/Pangnirtung', 'America/Port-au-Prince', 'America/Thunder_Bay', 'America/Toronto'] },
  { value: 'Eastern Daylight Time', offset: -4, text: '(UTC-04:00) Eastern Daylight Time (US & Canada)', utc: ['America/Detroit', 'America/Havana', 'America/Indiana/Petersburg', 'America/Indiana/Vincennes', 'America/Indiana/Winamac', 'America/Iqaluit', 'America/Kentucky/Monticello', 'America/Louisville', 'America/Montreal', 'America/Nassau', 'America/New_York', 'America/Nipigon', 'America/Pangnirtung', 'America/Port-au-Prince', 'America/Thunder_Bay', 'America/Toronto'] },
  { value: 'US Eastern Standard Time', offset: -5, text: '(UTC-05:00) Indiana (East)', utc: ['America/Indiana/Marengo', 'America/Indiana/Vevay', 'America/Indianapolis'] },
  { value: 'Venezuela Standard Time', offset: -4.5, text: '(UTC-04:30) Caracas', utc: ['America/Caracas'] },
  { value: 'Paraguay Standard Time', offset: -4, text: '(UTC-04:00) Asuncion', utc: ['America/Asuncion'] },
  { value: 'Atlantic Standard Time', offset: -3, text: '(UTC-04:00) Atlantic Time (Canada)', utc: ['America/Glace_Bay', 'America/Goose_Bay', 'America/Halifax', 'America/Moncton', 'America/Thule', 'Atlantic/Bermuda'] },
  { value: 'Central Brazilian Standard Time', offset: -4, text: '(UTC-04:00) Cuiaba', utc: ['America/Campo_Grande', 'America/Cuiaba'] },
  { value: 'SA Western Standard Time', offset: -4, text: '(UTC-04:00) Georgetown, La Paz, Manaus, San Juan', utc: ['America/Anguilla', 'America/Antigua', 'America/Aruba', 'America/Barbados', 'America/Blanc-Sablon', 'America/Boa_Vista', 'America/Curacao', 'America/Dominica', 'America/Grand_Turk', 'America/Grenada', 'America/Guadeloupe', 'America/Guyana', 'America/Kralendijk', 'America/La_Paz', 'America/Lower_Princes', 'America/Manaus', 'America/Marigot', 'America/Martinique', 'America/Montserrat', 'America/Port_of_Spain', 'America/Porto_Velho', 'America/Puerto_Rico', 'America/Santo_Domingo', 'America/St_Barthelemy', 'America/St_Kitts', 'America/St_Lucia', 'America/St_Thomas', 'America/St_Vincent', 'America/Tortola', 'Etc/GMT+4'] },
  { value: 'Pacific SA Standard Time', offset: -4, text: '(UTC-04:00) Santiago', utc: ['America/Santiago', 'Antarctica/Palmer'] },
  { value: 'Newfoundland Standard Time', offset: -2.5, text: '(UTC-03:30) Newfoundland', utc: ['America/St_Johns'] },
  { value: 'E. South America Standard Time', offset: -3, text: '(UTC-03:00) Brasilia', utc: ['America/Sao_Paulo'] },
  { value: 'Argentina Standard Time', offset: -3, text: '(UTC-03:00) Buenos Aires', utc: ['America/Argentina/Buenos_Aires', 'America/Buenos_Aires', 'America/Cordoba', 'America/Mendoza'] },
  { value: 'SA Eastern Standard Time', offset: -3, text: '(UTC-03:00) Cayenne, Fortaleza', utc: ['America/Araguaina', 'America/Belem', 'America/Cayenne', 'America/Fortaleza', 'America/Maceio', 'America/Paramaribo', 'America/Recife', 'America/Santarem', 'Antarctica/Rothera', 'Atlantic/Stanley', 'Etc/GMT+3'] },
  { value: 'Greenland Standard Time', offset: -3, text: '(UTC-03:00) Greenland', utc: ['America/Godthab'] },
  { value: 'Montevideo Standard Time', offset: -3, text: '(UTC-03:00) Montevideo', utc: ['America/Montevideo'] },
  { value: 'Bahia Standard Time', offset: -3, text: '(UTC-03:00) Salvador', utc: ['America/Bahia'] },
  { value: 'UTC-02', offset: -2, text: '(UTC-02:00) Coordinated Universal Time-02', utc: ['America/Noronha', 'Atlantic/South_Georgia', 'Etc/GMT+2'] },
  { value: 'Mid-Atlantic Standard Time', offset: -1, text: '(UTC-02:00) Mid-Atlantic - Old', utc: [] },
  { value: 'Azores Standard Time', offset: 0, text: '(UTC-01:00) Azores', utc: ['America/Scoresbysund', 'Atlantic/Azores'] },
  { value: 'Cape Verde Standard Time', offset: -1, text: '(UTC-01:00) Cape Verde Is.', utc: ['Atlantic/Cape_Verde', 'Etc/GMT+1'] },
  { value: 'Morocco Standard Time', offset: 1, text: '(UTC) Casablanca', utc: ['Africa/Casablanca', 'Africa/El_Aaiun'] },
  { value: 'UTC', offset: 0, text: '(UTC) Coordinated Universal Time', utc: ['America/Danmarkshavn', 'Etc/GMT'] },
  { value: 'GMT Standard Time', offset: 0, text: '(UTC) Edinburgh, London', utc: ['Europe/Isle_of_Man', 'Europe/Guernsey', 'Europe/Jersey', 'Europe/London'] },
  { value: 'British Summer Time', offset: 1, text: '(UTC+01:00) Edinburgh, London', utc: ['Europe/Isle_of_Man', 'Europe/Guernsey', 'Europe/Jersey', 'Europe/London'] },
  { value: 'GMT Standard Time', offset: 1, text: '(UTC) Dublin, Lisbon', utc: ['Atlantic/Canary', 'Atlantic/Faeroe', 'Atlantic/Madeira', 'Europe/Dublin', 'Europe/Lisbon'] },
  { value: 'Greenwich Standard Time', offset: 0, text: '(UTC) Monrovia, Reykjavik', utc: ['Africa/Abidjan', 'Africa/Accra', 'Africa/Bamako', 'Africa/Banjul', 'Africa/Bissau', 'Africa/Conakry', 'Africa/Dakar', 'Africa/Freetown', 'Africa/Lome', 'Africa/Monrovia', 'Africa/Nouakchott', 'Africa/Ouagadougou', 'Africa/Sao_Tome', 'Atlantic/Reykjavik', 'Atlantic/St_Helena'] },
  { value: 'W. Europe Standard Time', offset: 2, text: '(UTC+01:00) Amsterdam, Berlin, Bern, Rome, Stockholm, Vienna', utc: ['Arctic/Longyearbyen', 'Europe/Amsterdam', 'Europe/Andorra', 'Europe/Berlin', 'Europe/Busingen', 'Europe/Gibraltar', 'Europe/Luxembourg', 'Europe/Malta', 'Europe/Monaco', 'Europe/Oslo', 'Europe/Rome', 'Europe/San_Marino', 'Europe/Stockholm', 'Europe/Vaduz', 'Europe/Vatican', 'Europe/Vienna', 'Europe/Zurich'] },
  { value: 'Central Europe Standard Time', offset: 2, text: '(UTC+01:00) Belgrade, Bratislava, Budapest, Ljubljana, Prague', utc: ['Europe/Belgrade', 'Europe/Bratislava', 'Europe/Budapest', 'Europe/Ljubljana', 'Europe/Podgorica', 'Europe/Prague', 'Europe/Tirane'] },
  { value: 'Romance Standard Time', offset: 2, text: '(UTC+01:00) Brussels, Copenhagen, Madrid, Paris', utc: ['Africa/Ceuta', 'Europe/Brussels', 'Europe/Copenhagen', 'Europe/Madrid', 'Europe/Paris'] },
  { value: 'Central European Standard Time', offset: 2, text: '(UTC+01:00) Sarajevo, Skopje, Warsaw, Zagreb', utc: ['Europe/Sarajevo', 'Europe/Skopje', 'Europe/Warsaw', 'Europe/Zagreb'] },
  { value: 'W. Central Africa Standard Time', offset: 1, text: '(UTC+01:00) West Central Africa', utc: ['Africa/Algiers', 'Africa/Bangui', 'Africa/Brazzaville', 'Africa/Douala', 'Africa/Kinshasa', 'Africa/Lagos', 'Africa/Libreville', 'Africa/Luanda', 'Africa/Malabo', 'Africa/Ndjamena', 'Africa/Niamey', 'Africa/Porto-Novo', 'Africa/Tunis', 'Etc/GMT-1'] },
  { value: 'Namibia Standard Time', offset: 1, text: '(UTC+01:00) Windhoek', utc: ['Africa/Windhoek'] },
  { value: 'GTB Standard Time', offset: 3, text: '(UTC+02:00) Athens, Bucharest', utc: ['Asia/Nicosia', 'Europe/Athens', 'Europe/Bucharest', 'Europe/Chisinau'] },
  { value: 'Middle East Standard Time', offset: 3, text: '(UTC+02:00) Beirut', utc: ['Asia/Beirut'] },
  { value: 'Egypt Standard Time', offset: 2, text: '(UTC+02:00) Cairo', utc: ['Africa/Cairo'] },
  { value: 'Syria Standard Time', offset: 3, text: '(UTC+02:00) Damascus', utc: ['Asia/Damascus'] },
  { value: 'E. Europe Standard Time', offset: 3, text: '(UTC+02:00) E. Europe', utc: ['Asia/Nicosia', 'Europe/Athens', 'Europe/Bucharest', 'Europe/Chisinau', 'Europe/Helsinki', 'Europe/Kyiv', 'Europe/Mariehamn', 'Europe/Nicosia', 'Europe/Riga', 'Europe/Sofia', 'Europe/Tallinn', 'Europe/Uzhhorod', 'Europe/Vilnius', 'Europe/Zaporizhzhia'] },
  { value: 'South Africa Standard Time', offset: 2, text: '(UTC+02:00) Harare, Pretoria', utc: ['Africa/Blantyre', 'Africa/Bujumbura', 'Africa/Gaborone', 'Africa/Harare', 'Africa/Johannesburg', 'Africa/Kigali', 'Africa/Lubumbashi', 'Africa/Lusaka', 'Africa/Maputo', 'Africa/Maseru', 'Africa/Mbabane', 'Etc/GMT-2'] },
  { value: 'FLE Standard Time', offset: 3, text: '(UTC+02:00) Helsinki, Kyiv, Riga, Sofia, Tallinn, Vilnius', utc: ['Europe/Helsinki', 'Europe/Kyiv', 'Europe/Mariehamn', 'Europe/Riga', 'Europe/Sofia', 'Europe/Tallinn', 'Europe/Uzhhorod', 'Europe/Vilnius', 'Europe/Zaporizhzhia'] },
  { value: 'Turkey Standard Time', offset: 3, text: '(UTC+03:00) Istanbul', utc: ['Europe/Istanbul'] },
  { value: 'Israel Standard Time', offset: 3, text: '(UTC+02:00) Jerusalem', utc: ['Asia/Jerusalem'] },
  { value: 'Libya Standard Time', offset: 2, text: '(UTC+02:00) Tripoli', utc: ['Africa/Tripoli'] },
  { value: 'Jordan Standard Time', offset: 3, text: '(UTC+03:00) Amman', utc: ['Asia/Amman'] },
  { value: 'Arabic Standard Time', offset: 3, text: '(UTC+03:00) Baghdad', utc: ['Asia/Baghdad'] },
  { value: 'Kaliningrad Standard Time', offset: 3, text: '(UTC+02:00) Kaliningrad', utc: ['Europe/Kaliningrad'] },
  { value: 'Arab Standard Time', offset: 3, text: '(UTC+03:00) Kuwait, Riyadh', utc: ['Asia/Aden', 'Asia/Bahrain', 'Asia/Kuwait', 'Asia/Qatar', 'Asia/Riyadh'] },
  { value: 'E. Africa Standard Time', offset: 3, text: '(UTC+03:00) Nairobi', utc: ['Africa/Addis_Ababa', 'Africa/Asmera', 'Africa/Dar_es_Salaam', 'Africa/Djibouti', 'Africa/Juba', 'Africa/Kampala', 'Africa/Khartoum', 'Africa/Mogadishu', 'Africa/Nairobi', 'Antarctica/Syowa', 'Etc/GMT-3', 'Indian/Antananarivo', 'Indian/Comoro', 'Indian/Mayotte'] },
  { value: 'Moscow Standard Time', offset: 3, text: '(UTC+03:00) Moscow, St. Petersburg, Volgograd, Minsk', utc: ['Europe/Kirov', 'Europe/Moscow', 'Europe/Simferopol', 'Europe/Volgograd', 'Europe/Minsk'] },
  { value: 'Samara Time', offset: 4, text: '(UTC+04:00) Samara, Ulyanovsk, Saratov', utc: ['Europe/Astrakhan', 'Europe/Samara', 'Europe/Ulyanovsk'] },
  { value: 'Iran Standard Time', offset: 4.5, text: '(UTC+03:30) Tehran', utc: ['Asia/Tehran'] },
  { value: 'Arabian Standard Time', offset: 4, text: '(UTC+04:00) Abu Dhabi, Muscat', utc: ['Asia/Dubai', 'Asia/Muscat', 'Etc/GMT-4'] },
  { value: 'Azerbaijan Standard Time', offset: 5, text: '(UTC+04:00) Baku', utc: ['Asia/Baku'] },
  { value: 'Mauritius Standard Time', offset: 4, text: '(UTC+04:00) Port Louis', utc: ['Indian/Mahe', 'Indian/Mauritius', 'Indian/Reunion'] },
  { value: 'Georgian Standard Time', offset: 4, text: '(UTC+04:00) Tbilisi', utc: ['Asia/Tbilisi'] },
  { value: 'Caucasus Standard Time', offset: 4, text: '(UTC+04:00) Yerevan', utc: ['Asia/Yerevan'] },
  { value: 'Afghanistan Standard Time', offset: 4.5, text: '(UTC+04:30) Kabul', utc: ['Asia/Kabul'] },
  { value: 'West Asia Standard Time', offset: 5, text: '(UTC+05:00) Ashgabat, Tashkent', utc: ['Antarctica/Mawson', 'Asia/Aqtau', 'Asia/Aqtobe', 'Asia/Ashgabat', 'Asia/Dushanbe', 'Asia/Oral', 'Asia/Samarkand', 'Asia/Tashkent', 'Etc/GMT-5', 'Indian/Kerguelen', 'Indian/Maldives'] },
  { value: 'Yekaterinburg Time', offset: 5, text: '(UTC+05:00) Yekaterinburg', utc: ['Asia/Yekaterinburg'] },
  { value: 'Pakistan Standard Time', offset: 5, text: '(UTC+05:00) Islamabad, Karachi', utc: ['Asia/Karachi'] },
  { value: 'India Standard Time', offset: 5.5, text: '(UTC+05:30) Chennai, Kolkata, Mumbai, New Delhi', utc: ['Asia/Kolkata', 'Asia/Calcutta'] },
  { value: 'Sri Lanka Standard Time', offset: 5.5, text: '(UTC+05:30) Sri Jayawardenepura', utc: ['Asia/Colombo'] },
  { value: 'Nepal Standard Time', offset: 5.75, text: '(UTC+05:45) Kathmandu', utc: ['Asia/Kathmandu'] },
  { value: 'Central Asia Standard Time', offset: 6, text: '(UTC+06:00) Nur-Sultan (Astana)', utc: ['Antarctica/Vostok', 'Asia/Almaty', 'Asia/Bishkek', 'Asia/Qyzylorda', 'Asia/Urumqi', 'Etc/GMT-6', 'Indian/Chagos'] },
  { value: 'Bangladesh Standard Time', offset: 6, text: '(UTC+06:00) Dhaka', utc: ['Asia/Dhaka', 'Asia/Thimphu'] },
  { value: 'Myanmar Standard Time', offset: 6.5, text: '(UTC+06:30) Yangon (Rangoon)', utc: ['Asia/Rangoon', 'Indian/Cocos'] },
  { value: 'SE Asia Standard Time', offset: 7, text: '(UTC+07:00) Bangkok, Hanoi, Jakarta', utc: ['Antarctica/Davis', 'Asia/Bangkok', 'Asia/Hovd', 'Asia/Jakarta', 'Asia/Phnom_Penh', 'Asia/Pontianak', 'Asia/Saigon', 'Asia/Vientiane', 'Etc/GMT-7', 'Indian/Christmas'] },
  { value: 'N. Central Asia Standard Time', offset: 7, text: '(UTC+07:00) Novosibirsk', utc: ['Asia/Novokuznetsk', 'Asia/Novosibirsk', 'Asia/Omsk', 'Asia/Tomsk'] },
  { value: 'China Standard Time', offset: 8, text: '(UTC+08:00) Beijing, Chongqing, Hong Kong, Urumqi', utc: ['Asia/Hong_Kong', 'Asia/Macau', 'Asia/Shanghai'] },
  { value: 'North Asia Standard Time', offset: 8, text: '(UTC+08:00) Krasnoyarsk', utc: ['Asia/Krasnoyarsk'] },
  { value: 'Singapore Standard Time', offset: 8, text: '(UTC+08:00) Kuala Lumpur, Singapore', utc: ['Asia/Brunei', 'Asia/Kuala_Lumpur', 'Asia/Kuching', 'Asia/Makassar', 'Asia/Manila', 'Asia/Singapore', 'Etc/GMT-8'] },
  { value: 'W. Australia Standard Time', offset: 8, text: '(UTC+08:00) Perth', utc: ['Antarctica/Casey', 'Australia/Perth'] },
  { value: 'Taipei Standard Time', offset: 8, text: '(UTC+08:00) Taipei', utc: ['Asia/Taipei'] },
  { value: 'Ulaanbaatar Standard Time', offset: 8, text: '(UTC+08:00) Ulaanbaatar', utc: ['Asia/Choibalsan', 'Asia/Ulaanbaatar'] },
  { value: 'North Asia East Standard Time', offset: 8, text: '(UTC+08:00) Irkutsk', utc: ['Asia/Irkutsk'] },
  { value: 'Japan Standard Time', offset: 9, text: '(UTC+09:00) Osaka, Sapporo, Tokyo', utc: ['Asia/Dili', 'Asia/Jayapura', 'Asia/Tokyo', 'Etc/GMT-9', 'Pacific/Palau'] },
  { value: 'Korea Standard Time', offset: 9, text: '(UTC+09:00) Seoul', utc: ['Asia/Pyongyang', 'Asia/Seoul'] },
  { value: 'Cen. Australia Standard Time', offset: 9.5, text: '(UTC+09:30) Adelaide', utc: ['Australia/Adelaide', 'Australia/Broken_Hill'] },
  { value: 'AUS Central Standard Time', offset: 9.5, text: '(UTC+09:30) Darwin', utc: ['Australia/Darwin'] },
  { value: 'E. Australia Standard Time', offset: 10, text: '(UTC+10:00) Brisbane', utc: ['Australia/Brisbane', 'Australia/Lindeman'] },
  { value: 'AUS Eastern Standard Time', offset: 10, text: '(UTC+10:00) Canberra, Melbourne, Sydney', utc: ['Australia/Melbourne', 'Australia/Sydney'] },
  { value: 'West Pacific Standard Time', offset: 10, text: '(UTC+10:00) Guam, Port Moresby', utc: ['Antarctica/DumontDUrville', 'Etc/GMT-10', 'Pacific/Guam', 'Pacific/Port_Moresby', 'Pacific/Saipan', 'Pacific/Truk'] },
  { value: 'Tasmania Standard Time', offset: 10, text: '(UTC+10:00) Hobart', utc: ['Australia/Currie', 'Australia/Hobart'] },
  { value: 'Yakutsk Standard Time', offset: 9, text: '(UTC+09:00) Yakutsk', utc: ['Asia/Chita', 'Asia/Khandyga', 'Asia/Yakutsk'] },
  { value: 'Central Pacific Standard Time', offset: 11, text: '(UTC+11:00) Solomon Is., New Caledonia', utc: ['Antarctica/Macquarie', 'Etc/GMT-11', 'Pacific/Efate', 'Pacific/Guadalcanal', 'Pacific/Kosrae', 'Pacific/Noumea', 'Pacific/Ponape'] },
  { value: 'Vladivostok Standard Time', offset: 10, text: '(UTC+10:00) Vladivostok', utc: ['Asia/Ust-Nera', 'Asia/Vladivostok'] },
  { value: 'Sakhalin Standard Time', offset: 11, text: '(UTC+11:00) Sakhalin', utc: ['Asia/Sakhalin'] },
  { value: 'New Zealand Standard Time', offset: 12, text: '(UTC+12:00) Auckland, Wellington', utc: ['Antarctica/McMurdo', 'Pacific/Auckland'] },
  { value: 'UTC+12', offset: 12, text: '(UTC+12:00) Coordinated Universal Time+12', utc: ['Etc/GMT-12', 'Pacific/Funafuti', 'Pacific/Kwajalein', 'Pacific/Majuro', 'Pacific/Nauru', 'Pacific/Tarawa', 'Pacific/Wake', 'Pacific/Wallis'] },
  { value: 'Fiji Standard Time', offset: 12, text: '(UTC+12:00) Fiji', utc: ['Pacific/Fiji'] },
  { value: 'Magadan Standard Time', offset: 12, text: '(UTC+12:00) Magadan', utc: ['Asia/Anadyr', 'Asia/Kamchatka', 'Asia/Magadan', 'Asia/Srednekolymsk'] },
  { value: 'Kamchatka Standard Time', offset: 13, text: '(UTC+12:00) Petropavlovsk-Kamchatsky - Old', utc: ['Asia/Kamchatka'] },
  { value: 'Tonga Standard Time', offset: 13, text: "(UTC+13:00) Nuku'alofa", utc: ['Etc/GMT-13', 'Pacific/Enderbury', 'Pacific/Fakaofo', 'Pacific/Tongatapu'] },
  { value: 'Samoa Standard Time', offset: 13, text: '(UTC+13:00) Samoa', utc: ['Pacific/Apia'] },
];

function cityOf(iana) {
  const seg = iana.split('/').pop() || '';
  return seg.replace(/_/g, ' ');
}

// Resolve one Windows row to a single representative IANA zone:
// prefer the utc[] zone whose city appears in `text`, else the first non-Etc/
// zone, else utc[0]. Returns null for an empty utc[].
function resolveIana(entry) {
  const utc = entry.utc || [];
  if (utc.length === 0) return null;
  const text = (entry.text || '').toLowerCase();
  const cityMatch = utc.find((z) => {
    const c = cityOf(z).toLowerCase();
    return c && text.includes(c);
  });
  if (cityMatch) return cityMatch;
  return utc.find((z) => !z.startsWith('Etc/')) || utc[0];
}

// Derived, deduped option list: [{ value: <IANA>, label: <text>, utc: [...] }],
// one per distinct resolved zone, in the source (offset) order.
export const TIMEZONE_OPTIONS = (() => {
  const seen = new Set();
  const out = [];
  for (const entry of RAW_ZONES) {
    const iana = resolveIana(entry);
    if (!iana || seen.has(iana)) continue;
    seen.add(iana);
    out.push({ value: iana, label: entry.text, utc: entry.utc });
  }
  return out;
})();

// Map a stored IANA to the option value to show as selected: exact match first,
// then "some option's utc[] contains it" (so e.g. a stored Asia/Jakarta still
// selects the Bangkok/Hanoi/Jakarta option). Returns null if nothing matches.
export function matchTimezone(stored) {
  if (!stored) return null;
  const exact = TIMEZONE_OPTIONS.find((o) => o.value === stored);
  if (exact) return exact.value;
  const contains = TIMEZONE_OPTIONS.find((o) => (o.utc || []).includes(stored));
  return contains ? contains.value : null;
}
