function toggle_bios() {
	Module.ccall('toggle_bios', 'number', [], []);
}

var toggle_bios = document.getElementById('toggle-bios');
if(toggle_bios !== null)
	toggle_bios.addEventListener('click', toggle_bios);
