build-and-apply-frontend: 
	cd frontend && yarn build && cd - && rm -rf firmware/data/* && cp frontend/out/* firmware/data -r
