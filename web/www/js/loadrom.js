function read_server_side_file(file_path) {
	var xhr = new XMLHttpRequest(); 
	xhr.open("GET", file_path);
	xhr.responseType = "blob";
	xhr.onload = function()
	{
		var blob = xhr.response;
	
		var reader = new FileReader();
		reader.onload = function(e) {
		var contents = e.target.result;
		var buf = Module._malloc(contents.byteLength);
		var dataHeap = new Uint8Array(Module.HEAPU8.buffer, buf, contents.byteLength);
		dataHeap.set(new Uint8Array(contents));
		Module.ccall('load_rom', 'number', ['number', 'number'], [dataHeap.byteOffset, contents.byteLength]);
		Module._free(buf);
		};
		reader.readAsArrayBuffer(blob);
	}
	xhr.send();
}

function read_local_file(e) {
	var file = e.target.files[0];
	if (!file) {
		return;
	}
	load_file_to_memory(file);
}

document.getElementById('file-input')
  .addEventListener('change', read_local_file, false);
 
function file_load(r)
{
	document.getElementById('file-input').click();
}

document.getElementById('load-rom')
  .addEventListener('click', file_load, false);
  
function local_server_rom(r)
{
	var ddl = document.getElementById('dropdown-roms');
	var path = ddl.options[ddl.selectedIndex].value;
	read_server_side_file(path);
}

document.getElementById('server-roms').addEventListener('click', local_server_rom, false);

document.querySelector("body").addEventListener("dragover", function (ev) { ev.preventDefault(); });
document.querySelector("body").addEventListener("drop", function (ev) {
	console.log('File(s) dropped');

	// Prevent default behavior (Prevent file from being opened)
	ev.preventDefault();
    ev.stopPropagation();

	if (ev.dataTransfer.items) {
		// Use DataTransferItemList interface to access the file(s)
		for (var i = 0; i < ev.dataTransfer.items.length; i++) {
			// If dropped items aren't files, reject them
			if (ev.dataTransfer.items[i].kind === 'file') {
				var file = ev.dataTransfer.items[i].getAsFile();
				console.log(file.name);
				if(load_file_to_memory(file))
					break;
			}
		}
	} else {
		// Use DataTransfer interface to access the file(s)
		for (var i = 0; i < ev.dataTransfer.files.length; i++) {
			console.log(ev.dataTransfer.files[i].name);
			if(load_file_to_memory(file))
				break;
		}
	} 

	// Pass event to removeDragData for cleanup
	removeDragData(ev)
});

function removeDragData(ev) {
	if (ev.dataTransfer.items) {
		// Use DataTransferItemList interface to remove the drag data
		ev.dataTransfer.items.clear();
	} else {
		// Use DataTransfer interface to remove the drag data
		ev.dataTransfer.clearData();
	}
}

function load_file_to_memory(file) {
	var reader = new FileReader();
	reader.onload = function(e) {
		var contents = e.target.result;
		var buf = Module._malloc(contents.byteLength);
		var dataHeap = new Uint8Array(Module.HEAPU8.buffer, buf, contents.byteLength);
		dataHeap.set(new Uint8Array(contents));
		Module.ccall('load_rom', 'number', ['number', 'number'], [dataHeap.byteOffset, contents.byteLength]);
		Module._free(buf);
	};
	reader.readAsArrayBuffer(file);
	return true;
}