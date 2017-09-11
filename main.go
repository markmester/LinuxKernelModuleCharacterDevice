package main


import (
	"syscall"
	"fmt"
)

const (
	DEVICE = "/dev/chardev"
	BUFFER_LENGTH = 256
)

func on_err(err error) {
	if err != nil {
		fmt.Println(err.Error())
		panic(err)
	}
}


func main() {
	write_buffer := make([]byte, BUFFER_LENGTH)

	fmt.Println("Starting device test code example...")
	fd, err := syscall.Open(DEVICE, syscall.O_RDWR, 0777)
	on_err(err)

	copy(write_buffer[:], "some data")

	numbytes, err := syscall.Write(fd, []byte("test"))
	on_err(err)
	fmt.Printf("Sent %d bytes...\n", numbytes)

	_, err = syscall.Read(fd, write_buffer)
	on_err(err)
	fmt.Printf("Incoming message: %s\n",string(write_buffer))
}
