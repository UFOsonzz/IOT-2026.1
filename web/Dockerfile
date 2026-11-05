FROM node:20-alpine

WORKDIR /app

# Cài đặt dependencies trước để tận dụng Docker layer cache
COPY package.json ./
RUN npm install

# Copy toàn bộ mã nguồn giao diện
COPY . .

EXPOSE 5173

# Khởi chạy Vite Dev Server mở cổng cho Docker host
CMD ["npm", "run", "dev", "--", "--host", "0.0.0.0", "--port", "5173"]