function save_ram() {
	Module.ccall('check_save', 'number', [], []);
}
	
document.getElementById('save-ram')
  .addEventListener('click', save_ram);